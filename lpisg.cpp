#include "lpisg.h"

// Print content of buffer

#include <iostream>
template <typename type>
__attribute__((always_inline)) inline
void printbuf(const type *buf, unsigned n, const char *name = NULL) {

	if (name) printf("%s = [ ", name);
	else printf("[ ");
	while (n--) std::cout << *(buf++) << " ";
	printf("]\n");
}

#ifndef TWITTER

__attribute__((always_inline)) inline
void createedge(edge *g, agent *adj, agent v1, agent v2, edge e, IloEnv &env, IloFloatVarArray &ea) {

	//printf("%u -- %u\n", v1, v2);
	g[v1 * N + v2] = g[v2 * N + v1] = e;
	adj[v1 * N + (adj[v1 * N]++) + 1] = v2;
	adj[v2 * N + (adj[v2 * N]++) + 1] = v1;

	ostringstream ostr;
	ostr << "e_" << v1 << "," << v2;
	ea.add(IloFloatVar(env, MINEDGEVALUE, FLT_MAX, ostr.str().c_str()));
}

__attribute__((always_inline)) inline
edge scalefree(edge *g, agent *adj, const chunk *dr, IloEnv &env, IloFloatVarArray &ea) {

	edge ne = 0;
	agent deg[N] = {0};

	for (agent i = 1; i <= M; i++) {
		for (agent j = 0; j < i; j++) {
			createedge(g, adj, i, j, N + ne, env, ea);
			deg[i]++;
			deg[j]++;
			ne++;
		}
	}

	agent t = 0;

	for (agent i = M + 1; i < N; i++) {
		t &= ~((1UL << i) - 1);
		for (agent j = 0; j < M; j++) {
			agent d = 0;
			for (agent h = 0; h < i; h++)
				if (!((t >> h) & 1)) d += deg[h];
			if (d > 0) {
				int p = nextInt(d);
				agent q = 0;
				while (p >= 0) {
					if (!((t >> q) & 1)) p = p - deg[q];
					q++;
				}
				q--;
				t |= 1UL << q;
				createedge(g, adj, i, q, N + ne, env, ea);
				deg[i]++;
				deg[q]++;
				ne++;
			}
		}
	}

	for (agent i = 0; i < N; i++)
		QSORT(agent, adj + i * N + 1, adj[i * N], LTDR);

	return ne;
}

#endif

int main(int argc, char *argv[]) {

	unsigned seed = atoi(argv[1]);
	meter *sp = createsp(seed);

	agent dra[N] = {0};
	chunk dr[C] = {0};

	for (agent i = 0; i < D; i++)
		dra[i] = 1;

	shuffle(dra, N, sizeof(agent));

	for (agent i = 0; i < N; i++)
		if (dra[i]) SET(dr, i);

	// CPLEX environment and model

	IloEnv env;
	IloModel model(env);

	// Variables representing edge values

	IloFloatVarArray ea(env, N);
	IloFloatVarArray da(env);
	ostringstream ostr;

	for (agent i = 0; i < N; i++) {
		ostr << "e_" << i;
		ea[i] = IloFloatVar(env, MINEDGEVALUE, FLT_MAX, ostr.str().c_str());
		ostr.str("");
	}

	init(seed);
	edge *g = (edge *)calloc(N * N, sizeof(edge));
	agent *adj = (agent *)calloc(N * N, sizeof(agent));
	#ifdef DEBUG
	scalefree(g, adj, dr, env, ea);
	#endif

	#ifndef CSV
	puts("Creating LP model...");
	#endif

	#ifdef DEBUG
	puts("\nAdjacency lists");
	for (agent i = 0; i < N; i++)
		printbuf(adj + i * N + 1, adj[i * N]);
	puts("\nAdjacency matrix");
	for (agent i = 0; i < N; i++)
		printbuf(g + i * N, N);
	puts("");
	#endif

	// Create constraints

	const penny tv = constraints(g, adj, dr, sp, env, model, ea, da);

	// Create objective expression

	IloExpr expr(env);
	for (agent i = 0; i < da.getSize(); i++)
		expr += da[i];

	#ifdef DEBUG
	cout << expr << endl;
	#endif

	model.add(IloMinimize(env, expr));
	expr.end();

	#ifndef CSV
	puts("Starting CPLEX...\n");
	#endif

	IloCplex cplex(model);
	IloTimer timer(env);
	timer.start();

	#ifdef CSV
	cplex.setOut(env.getNullStream());
	#endif

	if (!cplex.solve()) {
		env.out() << "Unable to find a solution" << endl;
		exit(1);
	}

	timer.stop();
	double dif = cplex.getObjValue();

	#ifdef CSV
	printf("%u,%u,%u,%.2f,%.2f,%.2f,%.2f\n", N, DRIVERSPERC, seed, dif, (dif * 1E4) / tv, dif / da.getSize(), timer.getTime() * 1000);
	#else
	puts("Edge values:");
	for (edge i = 0; i < ea.getSize(); i++) {
		try {
			const double val = cplex.getValue(ea[i]);
			cout << ea[i].getName() << " = " << val << endl;
		}
		catch (IloException& e) {
			cout << ea[i].getName() << " never occurs in a feasible coalition" << endl;
			e.end();
		}
	}

	env.out() << "\nLP solution elapsed time = " << timer.getTime() * 1000 << "ms" << endl;
	printf("Overall difference = %.2f\n", dif);
	printf("Percentage difference = %.2f%%\n", (dif * 1E4) / tv);
	printf("Average difference = %.2f\n", dif / da.getSize());
	#endif

	env.end();
	free(adj);
	free(g);

	return 0;
}
