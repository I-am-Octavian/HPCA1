#include <iostream>
#include <vector>
#include <iomanip>
#include <cstdint>   // added

#include "benchmark.h"
#include "bitmap.h"
#include "builder.h"
#include "command_line.h"
#include "graph.h"
#include "platform_atomics.h"
#include "pvector.h"
#include "sliding_queue.h"
#include "timer.h"

#include <cstdint>

using namespace std;

// Global stats for final report (unchanged)
static int64_t g_traversed_edges = 0;
static double  g_bfs_time_sec    = 0.0;

// New: optimization toggles via simple argv parsing
static bool g_opt_visited = true;   // enable visited-byte optimization by default
static bool g_opt_prefetch = false; // prefetch off by default (iterator dependent)

// Metrics (extended): split visited-byte accesses from bitmap(front/curr)
struct BfsMemMetrics {
  int64_t col_ind_reads      = 0; // CSR neighbor index loads
  int64_t parent_reads       = 0; // parent/visited reads (parent array)
  int64_t parent_writes      = 0; // parent writes on discovery
  int64_t frontier_pushes    = 0; // queue enqueues
  int64_t bitmap_reads       = 0; // frontier/curr bitmap get_bit
  int64_t bitmap_writes      = 0; // frontier/curr bitmap set_bit
  int64_t visited_byte_reads = 0; // reads from visited[] byte array (new)
  int64_t visited_byte_writes= 0; // writes to visited[] byte array (new)
};
static BfsMemMetrics g_metrics;

// Optional: simple custom arg stripper so CLApp doesn't see our flags
static void StripCustomArgs(int& argc, char** argv) {
  int out = 1;
  for (int i = 1; i < argc; ++i) {
    string a = argv[i];
    if (a == "--opt-visited=0") g_opt_visited = false;
    else if (a == "--opt-visited=1") g_opt_visited = true;
    else if (a == "--opt-prefetch=1") g_opt_prefetch = true;
    else if (a == "--opt-prefetch=0") g_opt_prefetch = false;
    else argv[out++] = argv[i];
  }
  argc = out;
}

// Bottom-up step (unchanged semantics; metrics kept)
int64_t BUStep(const Graph &g, pvector<NodeID> &parent, Bitmap &front,
               Bitmap &next, int64_t &edges_visited) {
  int64_t awake_count = 0;
  next.reset();
  for (NodeID u = 0; u < g.num_nodes(); u++) {
    g_metrics.parent_reads++;                 // read parent[u] state
    if (parent[u] < 0) {
      for (NodeID v : g.in_neigh(u)) {
        edges_visited++;
        g_metrics.col_ind_reads++;            // in-neighbor index
        g_metrics.bitmap_reads++;             // front.get_bit
        if (front.get_bit(v)) {
          parent[u] = v;
          g_metrics.parent_writes++;          // write parent
          awake_count++;
          next.set_bit(u);
          g_metrics.bitmap_writes++;          // write next bitmap
          break;                              // early exit
        }
      }
    }
  }
  return awake_count;
}

// Top-down step with optional visited-byte fast path
int64_t TDStep(const Graph &g, pvector<NodeID> &parent,
               SlidingQueue<NodeID> &queue, int64_t &edges_visited,
               uint8_t* visited_bytes_or_null) {
  const bool use_visited = g_opt_visited && (visited_bytes_or_null != nullptr);
  uint8_t* visited = visited_bytes_or_null;
  int64_t scout_count = 0;
  QueueBuffer<NodeID> lqueue(queue);

  for (auto q_iter = queue.begin(); q_iter < queue.end(); q_iter++) {
    NodeID u = *q_iter;
    for (NodeID v : g.out_neigh(u)) {
      edges_visited++;
      g_metrics.col_ind_reads++;              // neighbor index load

      // Fast reject via visited byte before touching parent[v]
      if (use_visited) {
        g_metrics.visited_byte_reads++;
        if (visited[v]) continue;             // already visited: skip
      }

      // Fallback/confirm via parent array
      NodeID curr_val = parent[v];
      g_metrics.parent_reads++;               // parent read
      if (curr_val < 0) {
        // Found an undiscovered vertex; mark visited first (single-thread)
        if (use_visited) {
          if (!visited[v]) {
            visited[v] = 1;
            g_metrics.visited_byte_writes++;
          }
        }
        // Publish parent and enqueue
        if (compare_and_swap(parent[v], curr_val, u)) {
          lqueue.push_back(v);
          g_metrics.frontier_pushes++;
          scout_count += -curr_val;           // degree stored as negative
        }
      }
    }
  }
  lqueue.flush();
  return scout_count;
}

void QueueToBitmap(const SlidingQueue<NodeID> &queue, Bitmap &bm) {
  for (auto q_iter = queue.begin(); q_iter < queue.end(); q_iter++) {
    NodeID u = *q_iter;
    bm.set_bit_atomic(u);
    g_metrics.bitmap_writes++;
  }
}

void BitmapToQueue(const Graph &g, const Bitmap &bm,
                   SlidingQueue<NodeID> &queue) {
  QueueBuffer<NodeID> lqueue(queue);
  for (NodeID n = 0; n < g.num_nodes(); n++) {
    g_metrics.bitmap_reads++;
    if (bm.get_bit(n)) {
      lqueue.push_back(n);
      g_metrics.frontier_pushes++;
    }
  }
  lqueue.flush();
  queue.slide_window();
}

pvector<NodeID> InitParent(const Graph &g) {
  pvector<NodeID> parent(g.num_nodes());
  for (NodeID n = 0; n < g.num_nodes(); n++)
    parent[n] = g.out_degree(n) != 0 ? -g.out_degree(n) : -1;
  return parent;
}

pvector<NodeID> DOBFS(const Graph &g, NodeID source, bool logging_enabled = false,
                      int alpha = 15, int beta = 18) {
  if (logging_enabled) PrintStep("Source", static_cast<int64_t>(source));

  Timer t;
  t.Start();
  pvector<NodeID> parent = InitParent(g);
  t.Stop();
  if (logging_enabled) PrintStep("i", t.Seconds());

  parent[source] = source;

  // Optional visited-byte array (single-threaded)
  vector<uint8_t> visited_vec;
  uint8_t* visited_ptr = nullptr;
  if (g_opt_visited) {
    visited_vec.assign(g.num_nodes(), 0);
    visited_vec[source] = 1;
    visited_ptr = visited_vec.data();
    g_metrics.visited_byte_writes++;          // source mark
  }

  Timer t_total;
  t_total.Start();
  int64_t traversed_edges = 0;

  SlidingQueue<NodeID> queue(g.num_nodes());
  queue.push_back(source);
  queue.slide_window();
  Bitmap curr(g.num_nodes()); curr.reset();
  Bitmap front(g.num_nodes()); front.reset();
  int64_t edges_to_check = g.num_edges_directed();
  int64_t scout_count = g.out_degree(source);

  while (!queue.empty()) {
    if (scout_count > edges_to_check / alpha) {
      int64_t awake_count, old_awake_count;
      TIME_OP(t, QueueToBitmap(queue, front));
      if (logging_enabled) PrintStep("e", t.Seconds());
      awake_count = queue.size();
      queue.slide_window();
      do {
        t.Start();
        old_awake_count = awake_count;
        awake_count = BUStep(g, parent, front, curr, traversed_edges);
        front.swap(curr);
        t.Stop();
        if (logging_enabled) PrintStep("bu", t.Seconds(), awake_count);
      } while ((awake_count >= old_awake_count) ||
               (awake_count > g.num_nodes() / beta));
      TIME_OP(t, BitmapToQueue(g, front, queue));
      if (logging_enabled) PrintStep("c", t.Seconds());
      scout_count = 1;
    } else {
      t.Start();
      edges_to_check -= scout_count;
      scout_count = TDStep(g, parent, queue, traversed_edges, visited_ptr);
      queue.slide_window();
      t.Stop();
      if (logging_enabled) PrintStep("td", t.Seconds(), queue.size());
    }
  }

  for (NodeID n = 0; n < g.num_nodes(); n++)
    if (parent[n] < -1) parent[n] = -1;

  t_total.Stop();
  g_traversed_edges = traversed_edges;
  g_bfs_time_sec    = t_total.Seconds();
  return parent;
}

void PrintBFSStats(const Graph &g, const pvector<NodeID> &bfs_tree) {
  int64_t tree_size = 0, n_edges = 0;
  for (NodeID n : g.vertices()) {
    if (bfs_tree[n] >= 0) { n_edges += g.out_degree(n); tree_size++; }
  }
  cout << "BFS Tree has " << tree_size << " nodes and " << n_edges << " edges" << endl;
}

// BFS verifier (unchanged)
bool BFSVerifier(const Graph &g, NodeID source,
                 const pvector<NodeID> &parent) {
  pvector<int> depth(g.num_nodes(), -1);
  depth[source] = 0;
  vector<NodeID> to_visit;
  to_visit.reserve(g.num_nodes());
  to_visit.push_back(source);
  for (auto it = to_visit.begin(); it != to_visit.end(); it++) {
    NodeID u = *it;
    for (NodeID v : g.out_neigh(u)) {
      if (depth[v] == -1) {
        depth[v] = depth[u] + 1;
        to_visit.push_back(v);
      }
    }
  }
  for (NodeID u : g.vertices()) {
    if ((depth[u] != -1) && (parent[u] != -1)) {
      if (u == source) {
        if (!((parent[u] == u) && (depth[u] == 0))) {
          cout << "Source wrong" << endl;
          return false;
        }
        continue;
      }
      bool parent_found = false;
      for (NodeID v : g.in_neigh(u)) {
        if (v == parent[u]) {
          if (depth[v] != depth[u] - 1) {
            cout << "Wrong depths for " << u << " & " << v << endl;
            return false;
          }
          parent_found = true;
          break;
        }
      }
      if (!parent_found) {
        cout << "Couldn't find edge from " << parent[u] << " to " << u << endl;
        return false;
      }
    } else if (depth[u] != parent[u]) {
      cout << "Reachability mismatch" << endl;
      return false;
    }
  }
  return true;
}

int main(int argc, char* argv[]) {
  // Strip our custom flags so CLApp parses cleanly
  StripCustomArgs(argc, argv);

  CLApp cli(argc, argv, "breadth-first search");
  if (!cli.ParseArgs()) return -1;

  Builder b(cli);
  Graph g = b.MakeGraph();
  SourcePicker<Graph> sp(g, cli.start_vertex());
  auto BFSBound = [&sp,&cli] (const Graph &g) {
    return DOBFS(g, sp.PickNext(), cli.logging_en());
  };
  SourcePicker<Graph> vsp(g, cli.start_vertex());
  auto VerifierBound = [&vsp] (const Graph &g, const pvector<NodeID> &parent) {
    return BFSVerifier(g, vsp.PickNext(), parent);
  };
  BenchmarkKernel(cli, g, BFSBound, PrintBFSStats, VerifierBound);

  cout << "Traversed edges: " << g_traversed_edges << endl;
  cout << "BFS Time (s): " << fixed << setprecision(6) << g_bfs_time_sec << endl;

  // Byte model: count visited bytes separately from bitmaps; parent/frontier as before
  const double bytes_est =
      (double)g_metrics.col_ind_reads      * sizeof(NodeID)   +
      (double)g_metrics.parent_reads       * sizeof(NodeID)   +
      (double)g_metrics.parent_writes      * sizeof(NodeID)   +
      (double)g_metrics.frontier_pushes    * sizeof(NodeID)   +
      (double)g_metrics.bitmap_reads       * sizeof(uint64_t) +
      (double)g_metrics.bitmap_writes      * sizeof(uint64_t) +
      (double)g_metrics.visited_byte_reads * sizeof(uint8_t)  +
      (double)g_metrics.visited_byte_writes* sizeof(uint8_t);

  const double teps = (g_bfs_time_sec > 0.0) ? (double)g_traversed_edges / g_bfs_time_sec : 0.0;
  const double Ie_edges_per_byte = (bytes_est > 0.0) ? (double)g_traversed_edges / bytes_est : 0.0;

  cout << "Estimated bytes: " << fixed << setprecision(0) << bytes_est << endl;
  cout << "Edges per byte (Ie): " << setprecision(6) << Ie_edges_per_byte << endl;
  cout << "TEPS: " << setprecision(3) << teps << endl;

  // Debug counters (unchanged style)
  cerr << "[mem] col_reads=" << g_metrics.col_ind_reads
       << " parent_reads=" << g_metrics.parent_reads
       << " parent_writes=" << g_metrics.parent_writes
       << " frontier_pushes=" << g_metrics.frontier_pushes
       << " bitmap_reads=" << g_metrics.bitmap_reads
       << " bitmap_writes=" << g_metrics.bitmap_writes
       << " visited_reads=" << g_metrics.visited_byte_reads
       << " visited_writes=" << g_metrics.visited_byte_writes
       << endl;

  return 0;
}
