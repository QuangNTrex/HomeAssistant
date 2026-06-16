#include <iostream>
#include <vector>
#include <string>
#include <climits>
#include <algorithm>

using namespace std;

const int INF = 1e9;
const int V = 8;
const string name = "ABCDEFGH";

void printPath(const vector<int>& parent, int j) {
    if (parent[j] == -1) {
        cout << name[j];
        return;
    }
    printPath(parent, parent[j]);
    cout << " -> " << name[j];
}

void dijkstra(const vector<vector<int>>& graph, int src) {
    vector<int> dist(V, INF);
    vector<bool> sptSet(V, false);
    vector<int> parent(V, -1);

    dist[src] = 0;

    for (int count = 0; count < V - 1; ++count) {
        int u = -1;
        int min_dist = INF;
        for (int v = 0; v < V; ++v) {
            if (!sptSet[v] && dist[v] < min_dist) {
                min_dist = dist[v];
                u = v;
            }
        }

        if (u == -1) break;

        sptSet[u] = true;

        for (int v = 0; v < V; ++v) {
            if (!sptSet[v] && graph[u][v] != INF && dist[u] + graph[u][v] < dist[v]) {
                dist[v] = dist[u] + graph[u][v];
                parent[v] = u;
            }
        }
    }

    cout << "Shortest paths from A:" << endl;
    for (int i = 0; i < V; ++i) {
        cout << "To " << name[i] << ": Distance = ";
        if (dist[i] == INF) {
            cout << "INF" << endl;
        } else {
            cout << dist[i] << ", Path: ";
            printPath(parent, i);
            cout << endl;
        }
    }
}

int main() {
    // Adjacency matrix of the graph:
    // Vertices: A(0), B(1), C(2), D(3), E(4), F(5), G(6), H(7)
    vector<vector<int>> graph = {
        {0, 4, 6, 2, 5, INF, INF, INF},
        {4, 0, INF, 7, INF, INF, 8, INF},
        {6, INF, 0, 3, INF, 1, INF, INF},
        {2, 7, 3, 0, INF, 4, 5, INF},
        {5, INF, INF, INF, 0, 7, INF, INF},
        {INF, INF, 1, 4, 7, 0, 2, 3},
        {INF, 8, INF, 5, INF, 2, 0, 6},
        {INF, INF, INF, INF, INF, 3, 6, 0}
    };

    dijkstra(graph, 0);

    return 0;
}
