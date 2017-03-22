/*
 * PLUTO: A automatic parallelizer + locality optimizer (experimental)
 *
 * Copyright (C) 2007 - 2008 Uday Kumar Bondhugula
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * A copy of the GNU General Public Licence can be found in the
 * top-level directory of this program (`COPYING')
 *
 */
#ifdef HAVE_CONFIG_H
#include <pluto/config.h>
#endif

#include <string.h>
#include <pluto/pocc-driver.h>
#include <scoplib/scop.h>
#include <pluto/pluto.h>
#include <pluto/program.h>
#ifndef CLOOG_INT_GMP
# define CLOOG_INT_GMP
#endif
#include <cloog/cloog.h>
#include <pluto/post_transform.h>

#ifdef HAVE_LIBPOCC_UTILS


/* Set Cloog options according to pluto options. */
static
int pluto_set_cloog_options (PlutoProg *prog, s_pocc_utils_options_t* puoptions)
{

  Stmt *stmts = prog->stmts;
  int nstmts = prog->nstmts;

  CloogProgram *program ;
  CloogOptions *cloogOptions ;

  cloogOptions = puoptions->cloog_options;

  cloogOptions->name = "CLooG file produced by PLUTO";

  cloogOptions->compilable = 0;
  cloogOptions->callable = 0;
  /* These two options are now set by default with CLooG. */
  /*     cloogOptions->cpp = 1; */
  /*     cloogOptions->csp = 1; */
  cloogOptions->esp = 1;
  cloogOptions->strides = 1;

  if (plutoptions->cloogf >= 1 && plutoptions->cloogl >= 1) {
    cloogOptions->f = plutoptions->cloogf;
    cloogOptions->l = plutoptions->cloogl;
  }
  else
    {
      if (plutoptions->tile)   {
	int extra_rows_in_scattering = 0;
	if (plutoptions->tiling_in_scattering)
	  extra_rows_in_scattering = stmts[0].num_tiled_loops;
	if (plutoptions->ft == -1)  {
	  if (stmts[0].num_tiled_loops < 4)   {
	    cloogOptions->f = stmts[0].num_tiled_loops+1;
	    cloogOptions->l = stmts[0].trans->nrows - extra_rows_in_scattering;
	  }else{
	    cloogOptions->f = stmts[0].num_tiled_loops+1;
	    cloogOptions->l = stmts[0].trans->nrows - extra_rows_in_scattering;
	  }
	}else{
	  cloogOptions->f = stmts[0].num_tiled_loops+plutoptions->ft+1;
	  cloogOptions->l = stmts[0].trans->nrows - extra_rows_in_scattering;
	}
      }
      else
	{
	  /* Default */
	  cloogOptions->f = 1;
	  /* last level to optimize: infinity */
	  cloogOptions->l = -1;
	}
    }
  if (! plutoptions->silent)
    printf("[Pluto] using CLooG -f/-l options: %d %d\n", cloogOptions->f, cloogOptions->l);

  cloogOptions->name = "PLUTO-produced CLooG file";

  return EXIT_SUCCESS;
}

/* Fill the scop program with the computed transformation matrices. */
static
void pluto_fill_scop_transfo(scoplib_scop_p program, PlutoProg* prog)
{
    int i, j, k;
    int num_scat_dims;

    Stmt *stmts = prog->stmts;
    int nstmts = prog->nstmts;

    scoplib_statement_p stm = program->statement;
    /* Fill domains (may have been changed for tiling purposes). */
    for (i=0; i<nstmts; i++)    {
      scoplib_matrix_list_p elt = stm->domain;
      scoplib_matrix_p newdomain =
	scoplib_matrix_malloc (stmts[i].domain->nrows,
			       stmts[i].domain->ncols + 1);
        for (j=0; j<stmts[i].domain->nrows; j++)
	  {
	    SCOPVAL_set_si(newdomain->p[j][0], 1);
	    for (k=0; k<stmts[i].domain->ncols; k++)
	      SCOPVAL_set_si(newdomain->p[j][k + 1],
			     stmts[i].domain->val[j][k]);
	  }
	scoplib_matrix_free (elt->elt);
	elt->elt = newdomain;
	stm = stm->next;
    }
    /* Fill scattering functions */
    stm = program->statement;
    for (i = 0; i < nstmts; i++)
      {
        num_scat_dims = stmts[i].trans->nrows;
	stm->schedule =
	  scoplib_matrix_malloc(num_scat_dims,
				1 + stmts[i].trans->ncols + npar);
	for (j = 0; j < num_scat_dims; j++)
	  {
	    for (k=0; k<stmts[i].trans->ncols-1; k++)
	      SCOPVAL_set_si(stm->schedule->p[j][k + 1],
			     stmts[i].trans->val[j][k]);
	    SCOPVAL_set_si(stm->schedule->p[j][stm->schedule->NbColumns - 1],
			   stmts[i].trans->val[j][stmts[i].trans->ncols-1]);
	    if (plutoptions->tiling_in_scattering &&
		j >= num_scat_dims - 2 * stmts[i].num_tiled_loops)
	      SCOPVAL_set_si(stm->schedule->p[j][0], 1);
	  }
	stm = stm->next;
      }

}

int
pluto_pocc (scoplib_scop_p program,
	    PlutoOptions* ploptions,
	    s_pocc_utils_options_t* puoptions)
{
  // Ugly hack: store the option in the global variable. Required for macro
  // IF_DEBUGXX to work.
  plutoptions = ploptions;

  int i;

  /* IF_DEBUG(scoplib_scop_print_dot_scop(stdout, program)); */
  /* Convert scoplib scop to Pluto program */
  // scoplib_scop_print (stdout, program);
  PlutoProg *prog = scop_to_pluto_prog(program, ploptions);

  IF_DEBUG2(deps_print(stdout, prog->deps, prog->ndeps));
  IF_DEBUG2(stmts_print(stdout, prog->stmts, prog->nstmts));

  /* Create the data dependence graph */
  prog->ddg = ddg_create(prog);
  ddg_compute_scc(prog);

  int dim_sum=0;
  for (i=0; i<prog->nstmts; i++) {
    dim_sum += prog->stmts[i].dim;
  }

  /* Make options consistent */
  if (ploptions->multipipe == 1 && ploptions->parallel == 0)    {
    fprintf(stdout, "Warning: multipipe needs parallel to be on; turning on parallel\n");
    ploptions->parallel = 1;
  }

  /* Disable pre-vectorization if tile is not on */
  if (ploptions->tile == 0) {
    if (ploptions->prevector == 1)    {
      /* If code will not be tiled, pre-vectorization does not make
       * sense */
      fprintf(stdout, "[Pluto] Warning: disabling pre-vectorization (--tile is off)\n");
      ploptions->prevector = 0;
    }
  }
  if (! ploptions->silent)
    {
      fprintf(stdout, "[Pluto] Number of statements: %d\n", prog->nstmts);
      fprintf(stdout, "[Pluto] Total number of loops: %d\n", dim_sum);
      fprintf(stdout, "[Pluto] Number of deps: %d\n", prog->ndeps);
      fprintf(stdout, "[Pluto] Maximum domain dimensionality: %d\n", nvar);
      fprintf(stdout, "[Pluto] Number of parameters: %d\n", npar);
    }
  /* Auto transformation */
  /// FIXME: restablish that
/*   if (pluto_auto_transform(prog) == EXIT_ERROR) */
/*     return EXIT_ERROR; */

  pluto_auto_transform(prog);

  if (! ploptions->silent)
    {
      fprintf(stdout, "[Pluto] Affine transformations [<iter coeff's> <const>]\n\n");

      Stmt *stmts = prog->stmts;
      int nstmts = prog->nstmts;

      /* Print out the transformations */
      for (i=0; i<nstmts; i++) {
	fprintf(stdout, "T(S%d): ", i+1);
	int level;
	printf("(");
	for (level=0; level<prog->num_hyperplanes; level++) {
	  if (level > 0) printf(", ");
	  pretty_print_affine_function(stdout, &stmts[i], level);
	}
	printf(")\n");

	pluto_matrix_print(stdout, stmts[i].trans);
      }

      print_hyperplane_properties(prog->hProps, prog->num_hyperplanes);
    }

  if (ploptions->tile)   {
    pluto_tile(prog);
  }

  if (ploptions->parallel)   {
    int outermostBandStart, outermostBandEnd;
    getOutermostTilableBand(prog, &outermostBandStart, &outermostBandEnd);

    /* Obtain pipelined parallelization by skewing the tile space */
    bool retval = create_tile_schedule(prog, outermostBandStart, outermostBandEnd);

    /* Even if the user hasn't supplied --tile and there is only pipelined
     * parallelism, we will warn the user, but anyway do fine-grained
     * parallelization
     */
  if (! ploptions->silent)
    {
      if (retval && ploptions->tile == 0)   {
	printf("WARNING: --tile is not used and there is pipelined parallelism\n");
	printf("\t This leads to fine-grained parallelism;\n");
	printf("\t add --tile to the list of cmd-line options for a better code.\n");
      }
    }
  }

  if (ploptions->prevector) {
    pre_vectorize(prog);
  }else{
    /* Create an empty .vectorize file */
    fclose(fopen(".vectorize", "w"));
  }

  if (ploptions->tile)  {
    if (! ploptions->silent)
      {
	fprintf(stdout, "[Pluto] After tiling:\n");
	print_hyperplane_properties(prog->hProps, prog->num_hyperplanes);
      }
    if (! ploptions->silent)
      {
	fprintf(stdout, "[Pluto] Writing tiling information to file.\n");
      }

    FILE *paramsFP = fopen(".tiles", "w");
    if(paramsFP){
      int numH = prog->num_hyperplanes;
      int j;
      for(j = 0;  j< numH; j++){
      	switch(prog->hProps[j].type){
      	  case H_TILE_SPACE_LOOP:
      	    fprintf(paramsFP, "c%d --> ", j+1-1);
      	    fprintf(paramsFP, "tLoop \n");
      	    break;
	  default:
	    break;
	}
      }
    }
    fclose(paramsFP);
  }

  if (ploptions->parallel)  {
    /* Generate meta info for insertion of OpenMP pragmas */
    generate_openmp_pragmas(prog);
  }


  if (ploptions->unroll || ploptions->polyunroll)    {
    /* Will generate a .unroll file */
    /* plann needs a .params */
    FILE *paramsFP = fopen(".params", "w");
    if (paramsFP)   {
      int i;
      for (i=0; i<npar; i++)  {
	fprintf(paramsFP, "%s\n", prog->params[i]);
      }
      fclose(paramsFP);
    }
    detect_unrollable_loops(prog);
  }else{
    /* Create an empty .unroll file */
    fclose(fopen(".unroll", "w"));
  }

  if (ploptions->polyunroll)    {
    /* Experimental */
    for (i=0; i<prog->num_hyperplanes; i++)   {
      if (prog->hProps[i].unroll)  {
	unroll_phis(prog, i, ploptions->ufactor);
      }
    }
  }

/*   IF_DEBUG(printf("[Pluto] Generating Cloog file\n")); */
  pluto_fill_scop_transfo(program, prog);
  pluto_set_cloog_options(prog, puoptions);
  pluto_prog_free(prog, ploptions);

  return EXIT_SUCCESS;
}

#endif /* ! HAVE_LIBPOCC_UTILS */
