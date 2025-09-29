// Copyright (c) 2015,
// The Regents of the University of California (Regents)
// See LICENSE.txt for license details

#include <iostream>
#include <vector>
#include <iomanip>

#include "benchmark.h"
#include "bitmap.h"
#include "builder.h"
#include "command_line.h"
#include "graph.h"
#include "platform_atomics.h"
#include "pvector.h"
#include "sliding_queue.h"
#include "timer.h"

/*
GAP Benchmark Suite
Kernel: Breadth-First Search (BFS)
Author: Scott Beamer

Single-threaded variant that:
- Counts traversed edges as every neighbor examination in both top-down (push)
  and bottom-up (pull) phases.
- Measures only BFS traversal time (search time), not graph build/verify.
- Prints traversed edges and BFS time at the end.
*/

using namespace std;

// Global stats for final report
static int64_t g_traversed_edges = 0;
static double  g_bfs_time_sec    = 0.0;

int64_t BUStep(const Graph &g, pvector<NodeID> &parent, Bitmap &front,
               Bitmap &next, int64_t &edges_visited) {
  int64_t awake_count = 0;
  next.reset();
  for (NodeID u = 0; u < g.num_nodes(); u++) {
    if (parent[u] < 0) {
      for (NodeID v : g.in_neigh(u)) {
        edges_visited++;                 // count each in-neighbor check
        if (front.get_bit(v)) {
          parent[u] = v;
          awake_count++;
          next.set_bit(u);
          break;                         // stop after finding one parent
        }
      }
    }
  }
  return awake_count;
}

int64_t TDStep(const Graph &g, pvector<NodeID> &parent,
               SlidingQueue<NodeID> &queue, int64_t &edges_visited) {
  int64_t scout_count = 0;
  QueueBuffer<NodeID> lqueue(queue);
  for (auto q_iter = queue.begin(); q_iter < queue.end(); q_iter++) {
    NodeID u = *q_iter;
    for (NodeID v : g.out_neigh(u)) {
      edges_visited++;                   // count each out-neighbor check
      NodeID curr_val = parent[v];
      if (curr_val < 0) {
        if (compare_and_swap(parent[v], curr_val, u)) {
          lqueue.push_back(v);
          scout_count += -curr_val;      // degree stored as negative
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
  }
}

void BitmapToQueue(const Graph &g, const Bitmap &bm,
                   SlidingQueue<NodeID> &queue) {
  QueueBuffer<NodeID> lqueue(queue);
  for (NodeID n = 0; n < g.num_nodes(); n++)
    if (bm.get_bit(n))
      lqueue.push_back(n);
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
  if (logging_enabled)
    PrintStep("Source", static_cast<int64_t>(source));

  // Initialize parents (not counted in BFS time)
  Timer t;
  t.Start();
  pvector<NodeID> parent = InitParent(g);
  t.Stop();
  if (logging_enabled)
    PrintStep("i", t.Seconds());

  parent[source] = source;

  // Start BFS-only timing and set traversal counter
  Timer t_total;
  t_total.Start();
  int64_t traversed_edges = 0;

  SlidingQueue<NodeID> queue(g.num_nodes());
  queue.push_back(source);
  queue.slide_window();
  Bitmap curr(g.num_nodes());
  curr.reset();
  Bitmap front(g.num_nodes());
  front.reset();
  int64_t edges_to_check = g.num_edges_directed();
  int64_t scout_count = g.out_degree(source);
  while (!queue.empty()) {
    if (scout_count > edges_to_check / alpha) {
      int64_t awake_count, old_awake_count;
      TIME_OP(t, QueueToBitmap(queue, front));
      if (logging_enabled)
        PrintStep("e", t.Seconds());
      awake_count = queue.size();
      queue.slide_window();
      do {
        t.Start();
        old_awake_count = awake_count;
        awake_count = BUStep(g, parent, front, curr, traversed_edges);
        front.swap(curr);
        t.Stop();
        if (logging_enabled)
          PrintStep("bu", t.Seconds(), awake_count);
      } while ((awake_count >= old_awake_count) ||
               (awake_count > g.num_nodes() / beta));
      TIME_OP(t, BitmapToQueue(g, front, queue));
      if (logging_enabled)
        PrintStep("c", t.Seconds());
      scout_count = 1;
    } else {
      t.Start();
      edges_to_check -= scout_count;
      scout_count = TDStep(g, parent, queue, traversed_edges);
      queue.slide_window();
      t.Stop();
      if (logging_enabled)
        PrintStep("td", t.Seconds(), queue.size());
    }
  }

  for (NodeID n = 0; n < g.num_nodes(); n++)
    if (parent[n] < -1)
      parent[n] = -1;

  // Stop BFS-only timing and store globals for final print
  t_total.Stop();
  g_traversed_edges = traversed_edges;
  g_bfs_time_sec    = t_total.Seconds();
  return parent;
}

void PrintBFSStats(const Graph &g, const pvector<NodeID> &bfs_tree) {
  int64_t tree_size = 0;
  int64_t n_edges = 0;
  for (NodeID n : g.vertices()) {
    if (bfs_tree[n] >= 0) {
      n_edges += g.out_degree(n);
      tree_size++;
    }
  }
  cout << "BFS Tree has " << tree_size << " nodes and ";
  cout << n_edges << " edges" << endl;
}

// BFS verifier does a serial BFS from same source and asserts:
// - parent[source] = source
// - parent[v] = u  =>  depth[v] = depth[u] + 1 (except for source)
// - parent[v] = u  => there is edge from u to v
// - all vertices reachable from source have a parent
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
  CLApp cli(argc, argv, "breadth-first search");
  if (!cli.ParseArgs())
    return -1;
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

  // Final report for roofline modeling (single-thread)
  cout << "Traversed edges: " << g_traversed_edges << endl;
  cout << "BFS Time (s): " << fixed << setprecision(6) << g_bfs_time_sec << endl;
  return 0;
}
