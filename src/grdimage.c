/*--------------------------------------------------------------------
 *	$Id$
 *
 *	Copyright (c) 1991-2016 by P. Wessel, W. H. F. Smith, R. Scharroo, J. Luis and F. Wobbe
 *	See LICENSE.TXT file for copying and redistribution conditions.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU Lesser General Public License as published by
 *	the Free Software Foundation; version 3 or any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU Lesser General Public License for more details.
 *
 *	Contact info: gmt.soest.hawaii.edu
 *--------------------------------------------------------------------*/
/*
 * Author:	Paul Wessel
 * Date:	1-JAN-2010
 * Version:	5 API
 *
 * Brief synopsis: grdimage will read one (or 3) grid file and image the area
 * with optional shading.
 *
 */

#define THIS_MODULE_NAME	"grdimage"
#define THIS_MODULE_LIB		"core"
#define THIS_MODULE_PURPOSE	"Project grids or images and plot them on maps"
#define THIS_MODULE_KEYS	"<G{+,CC(,IG(,>X},>IA,<ID"

#include "gmt_dev.h"

#define GMT_PROG_OPTIONS "->BJKOPRUVXYcfnptxy" GMT_OPT("S")

/* Control structure for grdimage */

struct GRDIMAGE_CTRL {
	struct GRDIMG_In {
		bool active;
		bool do_rgb;
		char *file[3];
	} In;
	struct GRDIMG_Out {
		bool active;
		char *file;
	} Out;
	struct GRDIMG_C {	/* -C<cpt> or -C<color1>,<color2>[,<color3>,...] */
		bool active;
		char *file;
	} C;
	struct GRDIMG_D {	/* -D to read GDAL file */
		bool active;
		bool mode;	/* Use info of -R option to reference image */
	} D;
	struct GRDIMG_A {	/* -A to write a GDAL file or return image to API */
		bool active;
		bool return_image;
		char *file;
		char *driver;
	} A;
	struct GRDIMG_E {	/* -Ei|<dpi> */
		bool active;
		bool device_dpi;
		unsigned int dpi;
	} E;
	struct GRDIMG_G {	/* -G[f|b]<rgb> */
		bool active;
		double f_rgb[4];
		double b_rgb[4];
	} G;
	struct GRDIMG_I {	/* -I<intensfile>|<value> */
		bool active;
		bool constant;
		double value;
		char *file;
	} I;
	struct GRDIMG_M {	/* -M */
		bool active;
	} M;
	struct GRDIMG_N {	/* -N */
		bool active;
	} N;
	struct GRDIMG_Q {	/* -Q */
		bool active;
	} Q;
};

GMT_LOCAL void *New_Ctrl (struct GMT_CTRL *GMT) {	/* Allocate and initialize a new control structure */
	struct GRDIMAGE_CTRL *C;

	C = gmt_M_memory (GMT, NULL, 1, struct GRDIMAGE_CTRL);

	/* Initialize values whose defaults are not 0/false/NULL */

	C->G.b_rgb[0] = C->G.b_rgb[1] = C->G.b_rgb[2] = 1.0;

	return (C);
}

GMT_LOCAL void Free_Ctrl (struct GMT_CTRL *GMT, struct GRDIMAGE_CTRL *C) {	/* Deallocate control structure */
	int k;
	if (!C) return;
	for (k = 0; k < 3; k++) gmt_M_str_free (C->In.file[k]);	
	gmt_M_str_free (C->C.file);
	gmt_M_str_free (C->I.file);
	gmt_M_free (GMT, C);
}

GMT_LOCAL int usage (struct GMTAPI_CTRL *API, int level) {
	gmt_show_name_and_purpose (API, THIS_MODULE_LIB, THIS_MODULE_NAME, THIS_MODULE_PURPOSE);
	if (level == GMT_MODULE_PURPOSE) return (GMT_NOERROR);
	if (API->mode) {	/* External interface */
		GMT_Message (API, GMT_TIME_NONE, "usage: grdimage <grd_z>|<grd_r> <grd_g> <grd_b> %s [%s] [-A] [-C<cpt>]\n", GMT_J_OPT, GMT_B_OPT); 
		GMT_Message (API, GMT_TIME_NONE, "\t[-D[r]] [-Ei|<dpi>] [-G[f|b]<rgb>] [-I<intensgrid>|<value>] [-K] [-M] [-N] [-O] [-P] [-Q]\n");
	}
	else {
#ifdef HAVE_GDAL
		GMT_Message (API, GMT_TIME_NONE, "usage: grdimage <grd_z>|<grd_r> <grd_g> <grd_b> %s [%s] [-A<out_img=driver>] [-C<cpt>]\n",
		             GMT_J_OPT, GMT_B_OPT); 
		GMT_Message (API, GMT_TIME_NONE, "\t[-D[r]] [-Ei|<dpi>] [-G[f|b]<rgb>] [-I<intensgrid>|<value>] [-K] [-M] [-N] [-O] [-P] [-Q]\n");
#else
		GMT_Message (API, GMT_TIME_NONE, "usage: grdimage <grd_z>|<grd_r> <grd_g> <grd_b> %s [%s] [-C<cpt>] [-Ei[|<dpi>]]\n",
		             GMT_J_OPT, GMT_B_OPT);
		GMT_Message (API, GMT_TIME_NONE, "\t[-G[f|b]<rgb>] [-I<intensgrid>|<value>] [-K] [-M] [-N] [-O] [-P] [-Q]\n");
#endif
	}
	GMT_Message (API, GMT_TIME_NONE, "\t[%s] [-T] [%s] [%s]\n", GMT_Rgeo_OPT, GMT_U_OPT, GMT_V_OPT);
	GMT_Message (API, GMT_TIME_NONE, "\t[%s] [%s] [%s] [%s]\n\t[%s]\n\t[%s] [%s]\n\n", 
	             GMT_X_OPT, GMT_Y_OPT, GMT_c_OPT, GMT_f_OPT, GMT_n_OPT, GMT_p_OPT, GMT_t_OPT);

	if (level == GMT_SYNOPSIS) return (GMT_MODULE_SYNOPSIS);

	GMT_Message (API, GMT_TIME_NONE, "\t<grd_z> is data set to be plotted.  Its z-values are in user units and will be\n");
	GMT_Message (API, GMT_TIME_NONE, "\t  converted to rgb colors via the CPT.  Alternatively, give three separate\n");
	GMT_Message (API, GMT_TIME_NONE, "\t  grid files that contain the red, green, and blue components in the 0-255 range.\n");
	if (API->mode)	/* External interface */
		GMT_Message (API, GMT_TIME_NONE, "\t  If -D is used then <grd_z> is instead expected to be an image.\n");
#ifdef HAVE_GDAL
	else
		GMT_Message (API, GMT_TIME_NONE, "\t  If -D is used then <grd_z> is instead expected to be an image.\n");
#endif
	GMT_Option (API, "J-");
	GMT_Message (API, GMT_TIME_NONE, "\n\tOPTIONS:\n");
	if (API->mode)	/* External interface */
		GMT_Message (API, GMT_TIME_NONE, "\t-A Return a GMT raster image instead of a PostScript plot.\n");
	else {
#ifdef HAVE_GDAL
		GMT_Message (API, GMT_TIME_NONE, "\t-A Save image in a raster format instead of PostScript. Append =<driver> to select.\n");
		GMT_Message (API, GMT_TIME_NONE, "\t   the image format. The 'driver' is the driver code name used by GDAL. For example\n");
		GMT_Message (API, GMT_TIME_NONE, "\t   -Aimg.tif=GTiff will write a GeoTiff image. Note: any vector elements are lost. \n");
#endif
	}
	GMT_Option (API, "B-");
	GMT_Message (API, GMT_TIME_NONE, "\t-C Color palette file to convert z to rgb. Optionally, instead give name of a master cpt\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   to automatically assign 16 continuous colors over the data range [rainbow].\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   Yet another option is to specify -Ccolor1,color2[,color3,...] to build a\n");
    GMT_Message (API, GMT_TIME_NONE, "\t   linear continuous cpt from those colors automatically.\n");
	if (API->mode)	/* External interface */
		GMT_Message (API, GMT_TIME_NONE, "\t-D <grd_z> is an image instead of a grid. Append r to equate image region to -R region.\n");
#ifdef HAVE_GDAL
	else
		GMT_Message (API, GMT_TIME_NONE, "\t-D Use to read an image via GDAL. Append r to equate image region to -R region.\n");
#endif
	GMT_Message (API, GMT_TIME_NONE, "\t-E Set dpi for the projected grid which must be constructed [100]\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   if -Jx or -Jm is not selected [Default gives same size as input grid].\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   Give i to do the interpolation in PostScript at device resolution.\n");
	gmt_rgb_syntax (API->GMT, 'G', "Set transparency color for images that otherwise would result in 1-bit images.\n\t  ");
	GMT_Message (API, GMT_TIME_NONE, "\t-I Use illumination. Append name of intensity grid file.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   For a constant intensity, just give the value instead.\n");
	GMT_Option (API, "K");
	GMT_Message (API, GMT_TIME_NONE, "\t-M Force monochrome image.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t-N Do not clip image at the map boundary.\n");
	GMT_Option (API, "O,P");
	GMT_Message (API, GMT_TIME_NONE, "\t-Q Use PS Level 3 colormasking to make nodes with z = NaN transparent.\n");
	GMT_Option (API, "R");
	GMT_Option (API, "U,V,X,c,f,n,p,t,.");

	return (GMT_MODULE_USAGE);
}

GMT_LOCAL int parse (struct GMT_CTRL *GMT, struct GRDIMAGE_CTRL *Ctrl, struct GMT_OPTION *options) {
	/* This parses the options provided to grdimage and sets parameters in Ctrl.
	 * Note Ctrl has already been initialized and non-zero default values set.
	 * Any GMT common options will override values set previously by other commands.
	 * It also replaces any file names specified as input or output with the data ID
	 * returned when registering these sources/destinations with the API.
	 */

	unsigned int n_errors = 0, n_files = 0;
	struct GMT_OPTION *opt = NULL;
	struct GMTAPI_CTRL *API = GMT->parent;
	size_t n;

	for (opt = options; opt; opt = opt->next) {	/* Process all the options given */

		switch (opt->option) {
			case '<':	/* Input file (only one or three is accepted) */
				Ctrl->In.active = true;
				if (n_files >= 3) break;
				if (gmt_check_filearg (GMT, '<', opt->arg, GMT_IN, GMT_IS_GRID))
					Ctrl->In.file[n_files++] = strdup (opt->arg);
				else
					n_errors++;
				break;
			case '>':	/* Output file (probably for -A via external interface) */
				Ctrl->Out.active = true;
				if (Ctrl->Out.file == NULL)
					Ctrl->Out.file = strdup (opt->arg);
				else
					n_errors++;
				break;

			/* Processes program-specific parameters */

			case 'A':	/* Get image file name plus driver name to write via GDAL */
				Ctrl->A.active = true;
				if (API->mode) {	/* External interface only */
					if ((n = strlen (opt->arg)) > 0) {
						GMT_Report (API, GMT_MSG_NORMAL, "Syntax error -A: No output argument allowed\n");
						n_errors++;
					}
					Ctrl->A.return_image = true;
				}
				else if ((n = strlen (opt->arg)) == 0) {
					GMT_Report (API, GMT_MSG_NORMAL, "Syntax error -A: No output name provided\n");
					n_errors++;
				}
#ifdef HAVE_GDAL
				else {	/* Must give file and GDAL driver and this requires GDAL support */
					Ctrl->A.file = strdup (opt->arg);
					while (Ctrl->A.file[n] != '=' && n > 0) n--;
					if (n == 0) {
						GMT_Report (API, GMT_MSG_NORMAL, "Syntax error -A: Missing the required =driver name.\n");
						n_errors++;
					}
					else {
						Ctrl->A.file[n] = '\0';		/* Strip =driver from file name */
						Ctrl->A.driver = strdup (&Ctrl->A.file[n+1]);
					}
				}
#else
				else
						GMT_Report (API, GMT_MSG_NORMAL, "Syntax error -A: Requires building GMT with GDAL support.\n");
#endif
				break;

			case 'C':	/* CPT */
				Ctrl->C.active = true;
				gmt_M_str_free (Ctrl->C.file);
				Ctrl->C.file = strdup (opt->arg);
				break;

			case 'D':	/* Get via GDAL */
				Ctrl->D.active = true;
				Ctrl->D.mode = (opt->arg[0] == 'r');
				break;

			case 'E':	/* Sets dpi */
				Ctrl->E.active = true;
				if (opt->arg[0] == 'i')	/* Interpolate image to device resolution */
					Ctrl->E.device_dpi = true;
				else if (opt->arg[0] == '\0')	
					Ctrl->E.dpi = 100;	/* Default grid dpi */
				else
					Ctrl->E.dpi = atoi (opt->arg);
				break;
			case 'G':	/* 1-bit fore or background color for transparent masks */
				Ctrl->G.active = true;
				switch (opt->arg[0]) {
					case 'F':
					case 'f':
						if (gmt_getrgb (GMT, &opt->arg[1], Ctrl->G.f_rgb)) {
							gmt_rgb_syntax (GMT, 'G', " ");
							n_errors++;
						}
						else
							Ctrl->G.b_rgb[0] = -1;
						break;
					case 'B':
					case 'b':
						if (gmt_getrgb (GMT, &opt->arg[1], Ctrl->G.b_rgb)) {
							gmt_rgb_syntax (GMT, 'G', " ");
							n_errors++;
						}
						else
							Ctrl->G.f_rgb[0] = -1;
						break;
					default:	/* Same as -Gf */
						if (gmt_getrgb (GMT, opt->arg, Ctrl->G.f_rgb)) {
							gmt_rgb_syntax (GMT, 'G', " ");
							n_errors++;
						}
						else
							Ctrl->G.b_rgb[0] = -1;
						break;
				}
				break;
			case 'I':	/* Use intensity from grid or constant */
				Ctrl->I.active = true;
				if (!gmt_access (GMT, opt->arg, R_OK))	/* Got a file */
					Ctrl->I.file = strdup (opt->arg);
				else if (opt->arg[0] && !gmt_not_numeric (GMT, opt->arg)) {	/* Looks like a constant value */
					Ctrl->I.value = atof (opt->arg);
					Ctrl->I.constant = true;
				}
				else {
					GMT_Report (API, GMT_MSG_NORMAL, "Syntax error -I: Requires a valid grid file or a constant\n");
					n_errors++;
				}
				break;
			case 'M':	/* Monochrome image */
				Ctrl->M.active = true;
				break;
			case 'N':	/* Do not clip at map boundary */
				Ctrl->N.active = true;
				break;
			case 'Q':	/* PS3 colormasking */
				Ctrl->Q.active = true;
				break;

			default:	/* Report bad options */
				n_errors += gmt_default_error (GMT, opt->option);
				break;
		}
	}

	if (!GMT->common.J.active) {	/* When no projection specified, use fake linear projection */
		gmt_parse_common_options (GMT, "J", 'J', "X15c");
		GMT->common.J.active = true;
	}

	if (n_files == 3) Ctrl->In.do_rgb = true;
	if (Ctrl->D.active) {	/* Only OK with memory input or GDAL support */
		if (!gmt_M_file_is_memory (Ctrl->In.file[0])) {
#ifndef HAVE_GDAL
			GMT_Report (API, GMT_MSG_NORMAL, "Syntax error -D: Requires building GMT with GDAL support.\n");
#endif
		}	
	}
	n_errors += gmt_M_check_condition (GMT, !GMT->common.J.active, 
					"Syntax error: Must specify a map projection with the -J option\n");
	if (!API->mode) {	/* I.e, not an External interface */
		n_errors += gmt_M_check_condition (GMT, !(n_files == 1 || n_files == 3), 
		                                   "Syntax error: Must specify one (or three) input file(s)\n");
	}
	n_errors += gmt_M_check_condition (GMT, Ctrl->I.active && !Ctrl->I.constant && !Ctrl->I.file, 
	                                   "Syntax error -I option: Must specify intensity file or value\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->E.active && !Ctrl->E.device_dpi && Ctrl->E.dpi <= 0, 
	                                   "Syntax error -E option: dpi must be positive\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->G.f_rgb[0] < 0 && Ctrl->G.b_rgb[0] < 0, 
	                                   "Syntax error -G option: Only one of fore/back-ground can be transparent for 1-bit images\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->M.active && Ctrl->Q.active,
	                                   "Syntax error -Q option: Cannot use -M when doing colormasking\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->A.return_image && Ctrl->Out.file == NULL,
	                                   "Syntax error -A option: Must provide an output filename for image\n");
	return (n_errors ? GMT_PARSE_ERROR : GMT_NOERROR);
}

GMT_LOCAL void GMT_set_proj_limits (struct GMT_CTRL *GMT, struct GMT_GRID_HEADER *r, struct GMT_GRID_HEADER *g, bool projected) {
	/* Sets the projected extent of the grid given the map projection
	 * The extreme x/y coordinates are returned in r, and dx/dy, and
	 * n_columns/n_rows are set accordingly.  Not that some of these may change
	 * if gmt_project_init is called at a later stage */

	unsigned int i, k;
	bool all_lats = false, all_lons = false;
	double x, y;

	r->n_columns = g->n_columns;	r->n_rows = g->n_rows;
	r->registration = g->registration;
	r->n_bands = g->n_bands;

	/* By default, use entire plot region */

	gmt_M_memcpy (r->wesn, GMT->current.proj.rect, 4, double);
	
	if (GMT->current.proj.projection == GMT_GENPER && GMT->current.proj.g_width != 0.0) return;

	if (gmt_M_is_geographic (GMT, GMT_IN)) {
		all_lats = gmt_M_180_range (g->wesn[YHI], g->wesn[YLO]);
		all_lons = gmt_M_grd_is_global (GMT, g);
		if (all_lons && all_lats) return;	/* Whole globe */
	}
	
	/* Must search for extent along perimeter */

	r->wesn[XLO] = r->wesn[YLO] = +DBL_MAX;
	r->wesn[XHI] = r->wesn[YHI] = -DBL_MAX;
	k = (g->registration == GMT_GRID_NODE_REG) ? 1 : 0;
	
	for (i = 0; i < g->n_columns - k; i++) {	/* South and north sides */
		gmt_geo_to_xy (GMT, g->wesn[XLO] + i * g->inc[GMT_X], g->wesn[YLO], &x, &y);
		r->wesn[XLO] = MIN (r->wesn[XLO], x), r->wesn[XHI] = MAX (r->wesn[XHI], x);
		r->wesn[YLO] = MIN (r->wesn[YLO], y), r->wesn[YHI] = MAX (r->wesn[YHI], y);
		gmt_geo_to_xy (GMT, g->wesn[XHI] - i * g->inc[GMT_X], g->wesn[YHI], &x, &y);
		r->wesn[XLO] = MIN (r->wesn[XLO], x), r->wesn[XHI] = MAX (r->wesn[XHI], x);
		r->wesn[YLO] = MIN (r->wesn[YLO], y), r->wesn[YHI] = MAX (r->wesn[YHI], y);
	}
	for (i = 0; i < g->n_rows - k; i++) {	/* East and west sides */
		gmt_geo_to_xy (GMT, g->wesn[XLO], g->wesn[YHI] - i * g->inc[GMT_Y], &x, &y);
		r->wesn[XLO] = MIN (r->wesn[XLO], x), r->wesn[XHI] = MAX (r->wesn[XHI], x);
		r->wesn[YLO] = MIN (r->wesn[YLO], y), r->wesn[YHI] = MAX (r->wesn[YHI], y);
		gmt_geo_to_xy (GMT, g->wesn[XHI], g->wesn[YLO] + i * g->inc[GMT_Y], &x, &y);
		r->wesn[XLO] = MIN (r->wesn[XLO], x), r->wesn[XHI] = MAX (r->wesn[XHI], x);
		r->wesn[YLO] = MIN (r->wesn[YLO], y), r->wesn[YHI] = MAX (r->wesn[YHI], y);
	}
	if (projected) {
		if (all_lons) {	/* Full 360, use min/max for x */
			r->wesn[XLO] = GMT->current.proj.rect[XLO];	r->wesn[XHI] = GMT->current.proj.rect[XHI];
		}
		if (all_lats) {	/* Full -90/+90, use min/max for y */
			r->wesn[YLO] = GMT->current.proj.rect[YLO];	r->wesn[YHI] = GMT->current.proj.rect[YHI];
		}
	}
}

#define bailout(code) {gmt_M_free_options (mode); return (code);}
#define Return(code) {Free_Ctrl (GMT, Ctrl); gmt_end_module (GMT, GMT_cpy); bailout (code);}

int GMT_grdimage (void *V_API, int mode, void *args) {
	bool done, need_to_project, normal_x, normal_y, resampled = false, gray_only = false;
	bool nothing_inside = false, use_intensity_grid;
	unsigned int k, n_columns = 0, n_rows = 0, grid_registration = GMT_GRID_NODE_REG, n_grids;
	unsigned int colormask_offset = 0, try, row, actual_row, col;
	uint64_t node_RGBA = 0;		/* uint64_t for the RGB(A) image array. */
	uint64_t node, kk, nm, byte, dim[3] = {0, 0, 3};
	int index = 0, ks, error = 0;
	
	unsigned char *bitimage_8 = NULL, *bitimage_24 = NULL, *rgb_used = NULL, i_rgb[3];

	double dx, dy, x_side, y_side, x0 = 0.0, y0 = 0.0, rgb[4] = {0.0, 0.0, 0.0, 0.0};
	double *NaN_rgb = NULL, red[4] = {1.0, 0.0, 0.0, 0.0}, wesn[4], inc[2] = {1.0, 1.0};

	struct GMT_GRID *Grid_orig[3] = {NULL, NULL, NULL}, *Grid_proj[3] = {NULL, NULL, NULL};
	struct GMT_GRID *Intens_orig = NULL, *Intens_proj = NULL;
	struct GMT_PALETTE *P = NULL;
	struct GRDIMAGE_CTRL *Ctrl = NULL;
	struct GMT_CTRL *GMT = NULL, *GMT_cpy = NULL;	/* General GMT interal parameters */
	struct GMT_OPTION *options = NULL;
	struct PSL_CTRL *PSL = NULL;	/* General PSL interal parameters */
	struct GMT_GRID_HEADER *header_work = NULL;	/* Pointer to a GMT header for the image or grid */
	struct GMTAPI_CTRL *API = gmt_get_api_ptr (V_API);	/* Cast from void to GMTAPI_CTRL pointer */

	bool do_indexed = false;
	double *r_table = NULL, *g_table = NULL, *b_table = NULL;
	struct GMT_IMAGE *I = NULL, *Img_proj = NULL;		/* A GMT image datatype, if GDAL is used */
	struct GMT_IMAGE *Out = NULL;		/* A GMT image datatype, if external interface is used with -A */
	struct GMT_GRID *G2 = NULL;
	struct GMT_GDALWRITE_CTRL *to_GDALW = NULL;

	/*----------------------- Standard module initialization and parsing ----------------------*/

	if (API == NULL) return (GMT_NOT_A_SESSION);
	if (mode == GMT_MODULE_PURPOSE) return (usage (API, GMT_MODULE_PURPOSE));	/* Return the purpose of program */
	options = GMT_Create_Options (API, mode, args);	if (API->error) return (API->error);	/* Set or get option list */

	if (!options || options->option == GMT_OPT_USAGE) bailout (usage (API, GMT_USAGE));	/* Return the usage message */
	if (options->option == GMT_OPT_SYNOPSIS) bailout (usage (API, GMT_SYNOPSIS));	/* Return the synopsis */

	/* Parse the command-line arguments */

	GMT = gmt_begin_module (API, THIS_MODULE_LIB, THIS_MODULE_NAME, &GMT_cpy); /* Save current state */
	if (GMT_Parse_Common (API, GMT_PROG_OPTIONS, options)) Return (API->error);
	Ctrl = New_Ctrl (GMT);	/* Allocate and initialize a new control structure */
	if ((error = parse (GMT, Ctrl, options)) != 0) Return (error);

	/*---------------------------- This is the grdimage main code ----------------------------*/

	n_grids = (Ctrl->In.do_rgb) ? 3 : 1;
	use_intensity_grid = (Ctrl->I.active && !Ctrl->I.constant);	/* We want to use the intensity grid */

	/* Read the illumination grid header right away so we can use its region to set that of an image (if requested) */
	if (use_intensity_grid) {	/* Illumination grid wanted */

		GMT_Report (API, GMT_MSG_VERBOSE, "Allocates memory and read intensity file\n");

		if ((Intens_orig = GMT_Read_Data (API, GMT_IS_GRID, GMT_IS_FILE, GMT_IS_SURFACE, GMT_GRID_HEADER_ONLY, NULL, Ctrl->I.file, NULL)) == NULL) {	/* Get header only */
			Return (API->error);
		}
	}

	if (Ctrl->D.active) {
		/* One more test though */
		unsigned int mode = 0;
		if (Ctrl->D.mode && !GMT->common.R.active) {
			GMT_Report (API, GMT_MSG_NORMAL, "Warning: -Dr without -R makes no sense. Ignoring -Dr.\n");
			Ctrl->D.mode = false;
		}

		if (use_intensity_grid && GMT->common.R.active) {
			if (GMT->common.R.wesn[XLO] < Intens_orig->header->wesn[XLO] || GMT->common.R.wesn[XHI] > Intens_orig->header->wesn[XHI] || 
			    GMT->common.R.wesn[YLO] < Intens_orig->header->wesn[YLO] || GMT->common.R.wesn[YHI] > Intens_orig->header->wesn[YHI]) {
				GMT_Report (API, GMT_MSG_NORMAL, "Requested region exceeds illumination extent\n");
				Return (GMT_RUNTIME_ERROR);
			}
		}

		if (!Ctrl->D.mode && use_intensity_grid && !GMT->common.R.active)	/* Apply illumination to an image but no -R provided */
			gmt_M_memcpy (GMT->common.R.wesn, Intens_orig->header->wesn, 4, double);
		
		mode = GMT_GRID_ALL;
		if (API->mode && Ctrl->In.file[0] == NULL) mode = GMT_GRID_DATA_ONLY;	/* An in memory resource */
		if ((I = GMT_Read_Data(API, GMT_IS_IMAGE, GMT_IS_FILE, GMT_IS_SURFACE, mode, NULL, Ctrl->In.file[0], NULL)) == NULL) {
			Return (API->error);
		}

		if (!Ctrl->D.mode && !Ctrl->I.active && !GMT->common.R.active)	/* No -R or -I. Use image dimensions as -R */
			gmt_M_memcpy (GMT->common.R.wesn, I->header->wesn, 4, double);

		if ( (Ctrl->D.mode && GMT->common.R.active) || (!Ctrl->D.mode && use_intensity_grid) ) {
			gmt_M_memcpy (I->header->wesn, GMT->common.R.wesn, 4, double);
			/* Get actual size of each pixel */
			dx = gmt_M_get_inc (GMT, I->header->wesn[XLO], I->header->wesn[XHI], I->header->n_columns, I->header->registration);
			dy = gmt_M_get_inc (GMT, I->header->wesn[YLO], I->header->wesn[YHI], I->header->n_rows, I->header->registration);
			I->header->inc[GMT_X] = dx;	I->header->inc[GMT_Y] = dy;
			I->header->r_inc[GMT_X] = 1.0 / dx;	/* Get inverse increments to avoid divisions later */
			I->header->r_inc[GMT_Y] = 1.0 / dy;
		}

		Ctrl->In.do_rgb = (I->header->n_bands >= 3);
		n_grids = 0;	/* Flag that we are using a GMT_IMAGE */

		if (I->header->ProjRefPROJ4 != NULL)
			GMT_Report (API, GMT_MSG_VERBOSE, "Data projection (Proj4 type)\n\t%s\n", I->header->ProjRefPROJ4);

		header_work = I->header;	/* OK, that's what what we'll use to send to gmt_grd_setregion */
	}

	GMT_Report (API, GMT_MSG_VERBOSE, "Allocates memory and read data file\n");

	if (!Ctrl->D.active) {
		for (k = 0; k < n_grids; k++) {
			if ((Grid_orig[k] = GMT_Read_Data (API, GMT_IS_GRID, GMT_IS_FILE, GMT_IS_SURFACE, GMT_GRID_HEADER_ONLY, NULL, Ctrl->In.file[k], NULL)) == NULL) {	/* Get header only */
				Return (API->error);
			}
		}
		if (!Ctrl->C.active) Ctrl->C.active = true;	/* Use default CPT stuff */
	}

	if (n_grids) header_work = Grid_orig[0]->header;	/* OK, we are in GRID mode and this was not set further above. Do it now */

	if (n_grids && Ctrl->In.do_rgb) {	/* Must ensure all three grids are coregistered */
		if (!gmt_M_grd_same_region (GMT, Grid_orig[0], Grid_orig[1])) error++;
		if (!gmt_M_grd_same_region (GMT, Grid_orig[0], Grid_orig[2])) error++;
		if (!(Grid_orig[0]->header->inc[GMT_X] == Grid_orig[1]->header->inc[GMT_X] && Grid_orig[0]->header->inc[GMT_X] == 
			Grid_orig[2]->header->inc[GMT_X])) error++;
		if (!(Grid_orig[0]->header->n_columns == Grid_orig[1]->header->n_columns && Grid_orig[0]->header->n_columns == Grid_orig[2]->header->n_columns)) error++;
		if (!(Grid_orig[0]->header->n_rows == Grid_orig[1]->header->n_rows && Grid_orig[0]->header->n_rows == Grid_orig[2]->header->n_rows)) error++;
		if (!(Grid_orig[0]->header->registration == Grid_orig[1]->header->registration && Grid_orig[0]->header->registration == 
			Grid_orig[2]->header->registration)) error++;
		if (error) {
			GMT_Report (API, GMT_MSG_NORMAL, "The r, g, and b grids are not congruent\n");
			Return (GMT_RUNTIME_ERROR);
		}
	}

	/* Determine what wesn to pass to map_setup */

	if (!GMT->common.R.active && n_grids) gmt_M_memcpy (GMT->common.R.wesn, Grid_orig[0]->header->wesn, 4, double);

	if (gmt_M_err_pass (GMT, gmt_map_setup (GMT, GMT->common.R.wesn), "")) Return (GMT_PROJECTION_ERROR);
	
	/* Determine if grid is to be projected */

	need_to_project = (gmt_M_is_nonlinear_graticule (GMT) || Ctrl->E.dpi > 0);
	if (need_to_project) GMT_Report (API, GMT_MSG_DEBUG, "Projected grid is non-orthogonal, nonlinear, or dpi was changed\n");
	
	/* Determine the wesn to be used to read the grid file; or bail if file is outside -R */

	if (!gmt_grd_setregion (GMT, header_work, wesn, need_to_project * GMT->common.n.interpolant))
		nothing_inside = true;
	else if (use_intensity_grid && !gmt_grd_setregion (GMT, Intens_orig->header, wesn, need_to_project * GMT->common.n.interpolant))
		nothing_inside = true;

	if (nothing_inside) {
		/* No grid to plot; just do empty map and bail */
		if ((PSL = gmt_plotinit (GMT, options)) == NULL) Return (GMT_RUNTIME_ERROR);
		gmt_plane_perspective (GMT, GMT->current.proj.z_project.view_plane, GMT->current.proj.z_level);
		gmt_plotcanvas (GMT);	/* Fill canvas if requested */
		gmt_map_basemap (GMT);
		gmt_plane_perspective (GMT, -1, 0.0);
		gmt_plotend (GMT);
		Return (GMT_NOERROR);
	}

	if (n_grids) {
		n_columns = gmt_M_get_n (GMT, wesn[XLO], wesn[XHI], Grid_orig[0]->header->inc[GMT_X], Grid_orig[0]->header->registration);
		n_rows = gmt_M_get_n (GMT, wesn[YLO], wesn[YHI], Grid_orig[0]->header->inc[GMT_Y], Grid_orig[0]->header->registration);
	}

	if (Ctrl->D.active) {	/* Trust more on info from gdal to make it more stable against pixel vs grid registration troubles */
		n_columns = I->header->n_columns;
		n_rows = I->header->n_rows;
	}

	if (!Ctrl->A.active) {	/* Otherwise we are not writing any postscript */
		if ((PSL = gmt_plotinit (GMT, options)) == NULL) Return (GMT_RUNTIME_ERROR);
		gmt_plane_perspective (GMT, GMT->current.proj.z_project.view_plane, GMT->current.proj.z_level);
		gmt_plotcanvas (GMT);	/* Fill canvas if requested */
		if (!Ctrl->N.active) gmt_map_clip_on (GMT, GMT->session.no_rgb, 3);
	}

	/* Read data */

	for (k = 0; k < n_grids; k++) {
		if (GMT_Read_Data (API, GMT_IS_GRID, GMT_IS_FILE, GMT_IS_SURFACE, GMT_GRID_DATA_ONLY, wesn, Ctrl->In.file[k], Grid_orig[k]) == NULL) {	/* Get grid data */
			Return (API->error);
		}
	}
	
	/* If given, get intensity file or compute intensities */

	if (use_intensity_grid) {	/* Illumination wanted */

		GMT_Report (API, GMT_MSG_VERBOSE, "Allocates memory and read intensity file\n");

		/* Remember, the illumination header was already read at the top */
		if (GMT_Read_Data (API, GMT_IS_GRID, GMT_IS_FILE, GMT_IS_SURFACE, GMT_GRID_DATA_ONLY, wesn, Ctrl->I.file, Intens_orig) == NULL) {
			Return (API->error);	/* Get grid data */
		}
		if (n_grids && (Intens_orig->header->n_columns != Grid_orig[0]->header->n_columns ||
		                Intens_orig->header->n_rows != Grid_orig[0]->header->n_rows)) {
			GMT_Report (API, GMT_MSG_NORMAL, "Intensity file has improper dimensions!\n");
			Return (GMT_RUNTIME_ERROR);
		}

		if (Ctrl->D.active && (I->header->n_columns != Intens_orig->header->n_columns || I->header->n_rows != Intens_orig->header->n_rows)) {
			/* Resize illumination grid to the image's size */

			int object_ID;
			char in_string[GMT_STR16], out_string[GMT_STR16], cmd[GMT_BUFSIZ];
			/* Create option list, register Intens_orig as input source via reference */
			if ((object_ID = GMT_Register_IO (API, GMT_IS_GRID, GMT_IS_REFERENCE|GMT_IO_RESET, GMT_IS_SURFACE, GMT_IN, NULL, Intens_orig)) == GMT_NOTSET) 
				Return (API->error);
			if (GMT_Encode_ID (API, in_string, object_ID) != GMT_NOERROR) {
				Return (API->error);	/* Make filename with embedded object ID for grid G */
			}

			if ((object_ID = GMT_Register_IO (API, GMT_IS_GRID, GMT_IS_REFERENCE, GMT_IS_SURFACE, GMT_OUT, NULL, NULL)) == GMT_NOTSET) {
				Return (API->error);
			}
			if (GMT_Encode_ID (GMT->parent, out_string, object_ID) != GMT_NOERROR) {
				Return (API->error);	/* Make filename with embedded object ID for result grid G2 */
			}

			sprintf (cmd, "%s -G%s -I%d+/%d+", in_string, out_string, n_columns, n_rows);
			if (GMT_Call_Module (GMT->parent, "grdsample", GMT_MODULE_CMD, cmd) != GMT_NOERROR) return (API->error);	/* Do the resampling */
			if ((G2 = GMT_Retrieve_Data (API, object_ID)) == NULL) {
				Return (API->error);
			}
			if (GMT_Destroy_Data (API, &Intens_orig) != GMT_NOERROR) {
				Return (API->error);
			}
			Intens_orig = G2;
		}

	}

	if (need_to_project) {	/* Need to resample the grd file */
		int nx_proj = 0, ny_proj = 0;
		double inc[2] = {0.0, 0.0};
		GMT_Report (API, GMT_MSG_VERBOSE, "project grid files\n");

		if (Ctrl->E.dpi == 0) {	/* Use input # of nodes as # of projected nodes */
			nx_proj = n_columns;
			ny_proj = n_rows;
		}
		if (Ctrl->D.active) { 
			if ((Img_proj = GMT_Duplicate_Data (API, GMT_IS_IMAGE, GMT_DUPLICATE_NONE, I)) == NULL) Return (API->error);	/* Just to get a header we can change */
			grid_registration = GMT_GRID_PIXEL_REG;	/* Force pixel */
			GMT_set_proj_limits (GMT, Img_proj->header, I->header, need_to_project);
			gmt_M_err_fail (GMT, gmt_project_init (GMT, Img_proj->header, inc, nx_proj, ny_proj, Ctrl->E.dpi, grid_registration),
			                Ctrl->In.file[0]);
			if (Ctrl->A.active)
				for (k = 0; k < 3; k++) GMT->current.setting.color_patch[GMT_NAN][k] = 1.0;	/* For img GDAL write use white as bg color */
			gmt_set_grddim (GMT, Img_proj->header);
			if (GMT_Create_Data (API, GMT_IS_IMAGE, GMT_IS_SURFACE, GMT_GRID_DATA_ONLY, NULL, NULL, NULL, 0, 0, Img_proj) == NULL)
				Return (API->error);
			gmt_img_project (GMT, I, Img_proj, false);
			if (GMT_Destroy_Data (API, &I) != GMT_NOERROR) {
				Return (API->error);
			}
		}
		for (k = 0; k < n_grids; k++) {
			if (!Grid_proj[k] && (Grid_proj[k] = GMT_Duplicate_Data (API, GMT_IS_GRID, GMT_DUPLICATE_NONE, Grid_orig[k])) == NULL)
				Return (API->error);	/* Just to get a header we can change */
					
			GMT_set_proj_limits (GMT, Grid_proj[k]->header, Grid_orig[k]->header, need_to_project);
			if (grid_registration == GMT_GRID_NODE_REG)		/* Force pixel if dpi is set */
				grid_registration = (Ctrl->E.dpi > 0) ? GMT_GRID_PIXEL_REG : Grid_orig[k]->header->registration;
			gmt_M_err_fail (GMT, gmt_project_init (GMT, Grid_proj[k]->header, inc, nx_proj, ny_proj, Ctrl->E.dpi, grid_registration),
			                Ctrl->In.file[k]);
			gmt_set_grddim (GMT, Grid_proj[k]->header);
			if (GMT_Create_Data (API, GMT_IS_GRID, GMT_IS_SURFACE, GMT_GRID_DATA_ONLY, NULL, NULL, NULL, 0, 0, Grid_proj[k]) == NULL)
				Return (API->error);
			gmt_grd_project (GMT, Grid_orig[k], Grid_proj[k], false);
			if (GMT_Destroy_Data (API, &Grid_orig[k]) != GMT_NOERROR) {
				Return (API->error);
			}
		}
		if (use_intensity_grid) {
			if ((Intens_proj = GMT_Duplicate_Data (API, GMT_IS_GRID, GMT_DUPLICATE_NONE, Intens_orig)) == NULL) Return (API->error);	/* Just to get a header we can change */
			if (n_grids)
				gmt_M_memcpy (Intens_proj->header->wesn, Grid_proj[0]->header->wesn, 4, double);
			else
				gmt_M_memcpy (Intens_proj->header->wesn, Img_proj->header->wesn, 4, double);
			if (Ctrl->E.dpi == 0) {	/* Use input # of nodes as # of projected nodes */
				nx_proj = Intens_orig->header->n_columns;
				ny_proj = Intens_orig->header->n_rows;
			}
			gmt_M_err_fail (GMT, gmt_project_init (GMT, Intens_proj->header, inc, nx_proj, ny_proj, Ctrl->E.dpi, grid_registration),
			                Ctrl->I.file);
			gmt_set_grddim (GMT, Intens_proj->header);
			if (GMT_Create_Data (API, GMT_IS_GRID, GMT_IS_SURFACE, GMT_GRID_DATA_ONLY, NULL, NULL, NULL, 0, 0, Intens_proj) == NULL)
				Return (API->error);
			gmt_grd_project (GMT, Intens_orig, Intens_proj, false);
			if (GMT_Destroy_Data (API, &Intens_orig) != GMT_NOERROR) {
				Return (API->error);
			}
		}
		resampled = true;
	}
	else {	/* Simply set Grid_proj[i]/Intens_proj to point to Grid_orig[i]/Intens_orig */
		struct GMT_GRID_HEADER tmp_header;
		for (k = 0; k < n_grids; k++) {	/* Must get a copy so we can change one without affecting the other */
			gmt_M_memcpy (&tmp_header, Grid_orig[k]->header, 1, struct GMT_GRID_HEADER);
			Grid_proj[k] = Grid_orig[k];
			GMT_set_proj_limits (GMT, Grid_proj[k]->header, &tmp_header, need_to_project);
		}
		if (use_intensity_grid) Intens_proj = Intens_orig;
		if (n_grids)
			grid_registration = Grid_orig[0]->header->registration;
		else {
			gmt_M_memcpy (&tmp_header, I->header, 1, struct GMT_GRID_HEADER);
			Img_proj = I;
			GMT_set_proj_limits (GMT, Img_proj->header, &tmp_header, need_to_project);
		}
	}

	if (n_grids) {
		Grid_proj[0]->header->n_bands = 1;
		header_work = Grid_proj[0]->header;	/* Later when need to refer to the header, use this copy */
	}
	if (Ctrl->D.active)
		header_work = Img_proj->header;	/* Later when need to refer to the header, use this copy */

	nm = header_work->nm;
	n_columns = header_work->n_columns;
	n_rows = header_work->n_rows;

	/* Get/calculate a color palette file */

	if (!Ctrl->In.do_rgb) {
		if (Ctrl->C.active) {		/* Read palette file */
			if ((P = gmt_get_cpt (GMT, Ctrl->C.file, GMT_CPT_OPTIONAL, header_work->z_min, header_work->z_max)) == NULL) {
				Return (API->error);
			}
			gray_only = (P && P->is_gray);
		}
		else if (Ctrl->D.active) {
			uint64_t cpt_len[1] = {256};
			/* We won't use much of the next 'P' but we still need to use some of its fields */
			if ((P = GMT_Create_Data (API, GMT_IS_PALETTE, GMT_IS_NONE, 0, cpt_len, NULL, NULL, 0, 0, NULL)) == NULL) Return (API->error);
			P->model = GMT_RGB;
			if (I->colormap == NULL && !strncmp (I->ColorInterp, "Gray", 4)) {
				r_table = gmt_M_memory (GMT, NULL, 256, double);
				for (k = 0; k < 256; k++) r_table[k] = gmt_M_is255 (k);
				gray_only = true;
			}
			else if (I->colormap != NULL) {
				r_table = gmt_M_memory (GMT, NULL, 256, double);
				g_table = gmt_M_memory (GMT, NULL, 256, double);
				b_table = gmt_M_memory (GMT, NULL, 256, double);
				for (k = 0; k < 256; k++) {
					r_table[k] = gmt_M_is255 (I->colormap[k*4]);	/* 4 because color table is RGBA */
					g_table[k] = gmt_M_is255 (I->colormap[k*4 + 1]);
					b_table[k] = gmt_M_is255 (I->colormap[k*4 + 2]);
				}
				do_indexed = true;		/* Now it will be RGB */
				gray_only = false;
			}
		}
	}

	if (P && P->has_pattern) GMT_Report (API, GMT_MSG_VERBOSE, "Warning: Patterns in CPT only apply to -T\n");
	GMT_Report (API, GMT_MSG_VERBOSE, "Evaluate pixel colors\n");

	NaN_rgb = (P) ? P->bfn[GMT_NAN].rgb : GMT->current.setting.color_patch[GMT_NAN];
	if (Ctrl->Q.active) {
		if (gray_only) {
			GMT_Report (API, GMT_MSG_VERBOSE, "Your image is grayscale only but -Q requires 24-bit; image will be converted to 24-bit.\n");
			gray_only = false;
			NaN_rgb = red;	/* Arbitrarily pick red as the NaN color since image is gray only */
			gmt_M_memcpy (P->bfn[GMT_NAN].rgb, red, 4, double);
		}
		rgb_used = gmt_M_memory (GMT, NULL, 256*256*256, unsigned char);
	}
	if (Ctrl->M.active || gray_only)
		bitimage_8 = gmt_M_memory (GMT, NULL, nm, unsigned char);
	else {
		if (Ctrl->Q.active) colormask_offset = 3;
		bitimage_24 = gmt_M_memory (GMT, NULL, 3 * nm + colormask_offset, unsigned char);
		if (P && Ctrl->Q.active) {
			for (k = 0; k < 3; k++) bitimage_24[k] = gmt_M_u255 (P->bfn[GMT_NAN].rgb[k]);
		}
	}
	normal_x = !(GMT->current.proj.projection == GMT_LINEAR && !GMT->current.proj.xyz_pos[0] && !resampled);
	normal_y = !(GMT->current.proj.projection == GMT_LINEAR && !GMT->current.proj.xyz_pos[1] && !resampled);

	for (try = 0, done = false; !done && try < 2; try++) {	/* Evaluate colors at least once, or twice if -Q and we need to select another NaN color */
		for (row = 0, byte = colormask_offset; row < n_rows; row++) {
			actual_row = (normal_y) ? row : n_rows - row - 1;
			kk = gmt_M_ijpgi (header_work, actual_row, 0);
			if (Ctrl->D.active && row == 0) node_RGBA = kk;		/* First time per row equals 'node', after grows alone */
			for (col = 0; col < n_columns; col++) {	/* Compute rgb for each pixel */
				node = kk + (normal_x ? col : n_columns - col - 1);
				if (Ctrl->D.active) {
					if (!Ctrl->In.do_rgb) {
						rgb[0] = r_table[(int)Img_proj->data[node]];
						if (do_indexed) {
							rgb[1] = g_table[(int)Img_proj->data[node]];
							rgb[2] = b_table[(int)Img_proj->data[node]];
						}
					}
					else {
						for (k = 0; k < 3; k++) rgb[k] = (double)Img_proj->data[node_RGBA++]/255;
						if (Img_proj->header->n_bands == 4) node_RGBA++;
					}
				}
				else if (Ctrl->In.do_rgb) {
					for (k = 0; k < 3; k++) {
						if (gmt_M_is_fnan (Grid_proj[k]->data[node])) {	/* If one is NaN they are all assumed to be NaN */
							k = 3;	/* To exit the k-loop */
							gmt_M_rgb_copy (rgb, NaN_rgb);
							index = GMT_NAN - 3;	/* Ensures no illumination done later */
						}
						else {				/* Set color, let index = 0 so illuminate test will work */
							rgb[k] = gmt_M_is255 (Grid_proj[k]->data[node]);
							if (rgb[k] < 0.0) rgb[k] = 0.0; else if (rgb[k] > 1.0) rgb[k] = 1.0;	/* Clip */
							index = 0;
						}
					}
				}
				else
					index = gmt_get_rgb_from_z (GMT, P, Grid_proj[0]->data[node], rgb);

				if (Ctrl->I.active && index != GMT_NAN - 3) {
					if (!n_grids || Intens_proj->header->reset_pad) {	/* Here we are illuminating an image. Must recompute "node" with the gmt_M_ijp macro */
						node = gmt_M_ijp (Intens_proj->header, actual_row, 0) + (normal_x ? col : n_columns - col - 1);
					}
					if (use_intensity_grid)
						gmt_illuminate (GMT, Intens_proj->data[node], rgb);
					else
						gmt_illuminate (GMT, Ctrl->I.value, rgb);
				}
				
				if (P && gray_only)		/* Color table only has grays, pick r */
					bitimage_8[byte++] = gmt_M_u255 (rgb[0]);
				else if (Ctrl->M.active)	/* Convert rgb to gray using the gmt_M_yiq transformation */
					bitimage_8[byte++] = gmt_M_u255 (gmt_M_yiq (rgb));
				else {
					for (k = 0; k < 3; k++) bitimage_24[byte++] = i_rgb[k] = gmt_M_u255 (rgb[k]);
					if (Ctrl->Q.active && index != GMT_NAN - 3) /* Keep track of all r/g/b combinations used except for NaN */
						rgb_used[(i_rgb[0]*256 + i_rgb[1])*256+i_rgb[2]] = true;
				}
			}

			if (!n_grids) node_RGBA += header_work->n_bands * (header_work->pad[XLO] + header_work->pad[XHI]);
		}

		if (P && Ctrl->Q.active) {	/* Check that we found an unused r/g/b value so colormasking will work OK */
			index = (gmt_M_u255(P->bfn[GMT_NAN].rgb[0])*256 + gmt_M_u255(P->bfn[GMT_NAN].rgb[1]))*256 + gmt_M_u255(P->bfn[GMT_NAN].rgb[2]);
			if (rgb_used[index]) {	/* This r/g/b already appears in the image as a non-NaN color; we must find a replacement NaN color */
				for (index = 0, ks = -1; ks == -1 && index < 256*256*256; index++) if (!rgb_used[index]) ks = index;
				if (ks == -1) {
					GMT_Report (API, GMT_MSG_NORMAL, "Warning: Colormasking will fail as there is no unused color that can represent transparency\n");
					done = true;
				}
				else {	/* Pick the first unused color (i.e., k) and let it play the role of the NaN color for transparency */
					bitimage_24[0] = (unsigned char)(ks >> 16);
					bitimage_24[1] = (unsigned char)((ks >> 8) & 255);
					bitimage_24[2] = (unsigned char)(ks & 255);
					GMT_Report (API, GMT_MSG_VERBOSE, "Warning: transparency color reset from %s to color %d/%d/%d\n", 
						gmt_putrgb (GMT, P->bfn[GMT_NAN].rgb), (int)bitimage_24[0], (int)bitimage_24[1], (int)bitimage_24[2]);
					for (k = 0; k < 3; k++) P->bfn[GMT_NAN].rgb[k] = gmt_M_is255 (bitimage_24[k]);	/* Set new NaN color */
				}	
			}
		}
		else
			done = true;
	}
	if (Ctrl->Q.active) gmt_M_free (GMT, rgb_used);
	
	for (k = 1; k < n_grids; k++) {	/* Not done with Grid_proj[0] yet, hence we start loop at k = 1 */
		if (need_to_project && GMT_Destroy_Data (API, &Grid_proj[k]) != GMT_NOERROR) {
			GMT_Report (API, GMT_MSG_NORMAL, "Failed to free Grid_proj[k]\n");
		}
	}
	if (use_intensity_grid) {
		if (need_to_project || !n_grids) {
			if (GMT_Destroy_Data (API, &Intens_proj) != GMT_NOERROR) {
				GMT_Report (API, GMT_MSG_NORMAL, "Failed to free Intens_proj\n");
			}
		}
	}
	
	/* Get actual size of each pixel */
	dx = gmt_M_get_inc (GMT, header_work->wesn[XLO], header_work->wesn[XHI], header_work->n_columns, header_work->registration);
	dy = gmt_M_get_inc (GMT, header_work->wesn[YLO], header_work->wesn[YHI], header_work->n_rows, header_work->registration);

	if (Ctrl->A.active && !Ctrl->A.return_image) {
		int	id, k;
		unsigned int this_proj = GMT->current.proj.projection;
		to_GDALW = gmt_M_memory (GMT, NULL, 1, struct GMT_GDALWRITE_CTRL);
		to_GDALW->driver = Ctrl->A.driver;
		to_GDALW->type = strdup("byte");
		to_GDALW->P.ProjectionRefPROJ4 = NULL;
		to_GDALW->flipud = 0;
		to_GDALW->geog = 0;
		to_GDALW->n_columns = (int)n_columns;
		to_GDALW->n_rows = (int)n_rows;
		if (Ctrl->D.active)
			to_GDALW->n_bands = (int)MIN(Img_proj->header->n_bands, 3);	/* Transparency not accounted yet */
		else if ((P && gray_only) || Ctrl->M.active)
			to_GDALW->n_bands = 1;
		else
			to_GDALW->n_bands = 3;
		to_GDALW->registration = 1;
		if (!need_to_project) {
			to_GDALW->ULx = GMT->common.R.wesn[XHI];
			to_GDALW->ULy = GMT->common.R.wesn[YHI];
			to_GDALW->x_inc = dx;
			to_GDALW->y_inc = dy;
		}
		else {
			to_GDALW->ULx = GMT->current.proj.rect_m[XLO];
			to_GDALW->ULy = GMT->current.proj.rect_m[YHI];
			to_GDALW->x_inc = (GMT->current.proj.rect_m[XHI] - GMT->current.proj.rect_m[XLO]) / n_columns;
			to_GDALW->y_inc = (GMT->current.proj.rect_m[YHI] - GMT->current.proj.rect_m[YLO]) / n_rows;
		}

		for (k = 0, id = -1; id == -1 && k < GMT_N_PROJ4; k++) 
			if (GMT->current.proj.proj4[k].id == this_proj) id = k;
		if (id >= 0) {			/* Valid projection for creating world file info */
			to_GDALW->P.active = true;
			to_GDALW->P.ProjectionRefPROJ4 = gmt_export2proj4 (GMT);
			if (to_GDALW->P.ProjectionRefPROJ4[1] == 'x' && to_GDALW->P.ProjectionRefPROJ4[2] == 'y')	/* -JX. Forget conversion */
				to_GDALW->P.active = false;
		}
	}

	/* Set lower left position of image on map */

	x0 = header_work->wesn[XLO];	y0 = header_work->wesn[YLO];
	if (grid_registration == GMT_GRID_NODE_REG) {	/* Grid registration, move 1/2 pixel down/left */
		x0 -= 0.5 * dx;
		y0 -= 0.5 * dy;
	}

	x_side = dx * header_work->n_columns;
	y_side = dy * header_work->n_rows;

	if (P && gray_only) 
		for (kk = 0, P->is_bw = true; P->is_bw && kk < nm; kk++) 
			if (!(bitimage_8[kk] == 0 || bitimage_8[kk] == 255)) P->is_bw = false;

	if (P && P->is_bw) {	/* Can get away with 1 bit image but must rewrite the byte to 8 image bits */
		int nx8, shift, b_or_w, nx_pixels, k8;
		unsigned char *bit = NULL;

		GMT_Report (API, GMT_MSG_VERBOSE, "Creating 1-bit B/W image\n");

		nx8 = irint (ceil (n_columns / 8.0));	/* Image width must equal a multiple of 8 bits */
		nx_pixels = nx8 * 8;
		bit = gmt_M_memory (GMT, NULL, nx8 * n_rows, unsigned char);

		for (row = k = k8 = 0; row < n_rows; row++) {
			shift = 0; byte = 0;
			for (col = 0; col < n_columns; col++, k++) {
				b_or_w = (bitimage_8[k] == 255);
				byte |= b_or_w;
				shift++;
				if (shift == 8) {	/* Time to dump out byte */
					bit[k8++] = (unsigned char) byte;
					byte = shift = 0;
				}
				else
					byte <<= 1;	/* Move the bits we have so far 1 step to the left */
			}
			if (shift) {	/* Set the remaining bits in this bit to white */
				byte |= 1;
				shift++;
				while (shift < 8) {
					byte <<= 1;
					byte |= 1;
					shift++;
				}
				bit[k8++] = (unsigned char) byte;
			}
		}

		x_side = nx_pixels * dx;
		PSL_plotbitimage (PSL, x0, y0, x_side, y_side, PSL_BL, bit, nx_pixels, n_rows, Ctrl->G.f_rgb, Ctrl->G.b_rgb);
		gmt_M_free (GMT, bit);
	}
	else if ((P && gray_only) || Ctrl->M.active) {	/* Here we have a 1-layer 8 bit image */
		if (Ctrl->A.active) {
			if (Ctrl->A.return_image) {	/* Create a GMT_IMAGE with 1 band and write output */
				dim[GMT_Z] = 1;
				GMT_Report (API, GMT_MSG_VERBOSE, "Creating 8-bit grayshade GMT_IMAGE object\n");
				wesn[XLO] = x0;	wesn[XHI] = x_side; wesn[YLO] = y0;	wesn[YHI] = y_side; inc[GMT_X] = dx;	inc[GMT_Y] = dy;
				if ((Out = GMT_Create_Data (API, GMT_IS_IMAGE, GMT_IS_SURFACE, GMT_GRID_HEADER_ONLY, dim, wesn, inc, grid_registration, 0, NULL)) == NULL)
					Return (API->error);
				Out->data = bitimage_8;	/* Pass out the byte data */
				bitimage_8 = NULL;	/* So we dont free this memory on exit */
				if (GMT_Write_Data (API, GMT_IS_IMAGE, GMT_IS_FILE, GMT_IS_SURFACE, GMT_GRID_ALL, NULL, Ctrl->Out.file, Out) != GMT_NOERROR)
					Return (API->error);
			}
#ifdef HAVE_GDAL
			else {
				GMT_Report (API, GMT_MSG_VERBOSE, "Creating 8-bit grayshade image via GDAL\n");
				to_GDALW->data = bitimage_8;
				gmt_gdalwrite(GMT, Ctrl->A.file, to_GDALW);
			}
#endif
		}
		else {
			GMT_Report (API, GMT_MSG_VERBOSE, "Creating 8-bit grayshade image\n");
			PSL_plotcolorimage (PSL, x0, y0, x_side, y_side, PSL_BL, bitimage_8, n_columns, n_rows, (Ctrl->E.device_dpi ? -8 : 8));
		}
	}
	else {	/* 24-bit image */
		if (Ctrl->A.active) {
			if (Ctrl->A.return_image) {	/* Create a GMT_IMAGE with 3 bands and write output */
				GMT_Report (API, GMT_MSG_VERBOSE, "Creating 24-bit color GMT_IMAGE object\n");
				wesn[XLO] = x0;	wesn[XHI] = x_side; wesn[YLO] = y0;	wesn[YHI] = y_side; inc[GMT_X] = dx;	inc[GMT_Y] = dy;
				if ((Out = GMT_Create_Data (API, GMT_IS_IMAGE, GMT_IS_SURFACE, GMT_GRID_HEADER_ONLY, dim, wesn, inc, grid_registration, 0, NULL)) == NULL)
					Return (API->error);
				Out->data = bitimage_24;	/* Pass out the 3*byte data */
				bitimage_24 = NULL;	/* So we dont free this memory on exit */
				if (GMT_Write_Data (API, GMT_IS_IMAGE, GMT_IS_FILE, GMT_IS_SURFACE, GMT_GRID_ALL, NULL, Ctrl->Out.file, Out) != GMT_NOERROR)
					Return (API->error);
			}
#ifdef HAVE_GDAL
			else {
				GMT_Report (API, GMT_MSG_VERBOSE, "Creating 24-bit color image via GDAL\n");
				to_GDALW->data = bitimage_24;
				gmt_gdalwrite(GMT, Ctrl->A.file, to_GDALW);
			}
#endif
		}
		else {
			GMT_Report (API, GMT_MSG_VERBOSE, "Creating 24-bit color image\n");
			PSL_plotcolorimage (PSL, x0, y0, x_side, y_side, PSL_BL, bitimage_24, (Ctrl->Q.active ? -1 : 1) * 
		    	n_columns, n_rows, (Ctrl->E.device_dpi ? -24 : 24));
		}
	}

	if (!Ctrl->A.active) {
		if (!Ctrl->N.active) gmt_map_clip_off (GMT);

		gmt_map_basemap (GMT);
		gmt_plane_perspective (GMT, -1, 0.0);
		gmt_plotend (GMT);
	}

	/* Free bitimage arrays. gmt_M_free will not complain if they have not been used (NULL) */
	gmt_M_free (GMT, bitimage_8);
	gmt_M_free (GMT, bitimage_24);

	if (need_to_project && n_grids && GMT_Destroy_Data (API, &Grid_proj[0]) != GMT_NOERROR) {
		GMT_Report (API, GMT_MSG_NORMAL, "Failed to free Grid_proj[0]\n");
	}

	if (Ctrl->D.active) {
		gmt_M_free (GMT, r_table);
		if (g_table) {
			gmt_M_free (GMT, g_table);
			gmt_M_free (GMT, b_table);
		}
		if (GMT_Destroy_Data (API, &Img_proj) != GMT_NOERROR) {
			Return (API->error);
		}
		if (!Ctrl->C.active && GMT_Destroy_Data (API, &P) != GMT_NOERROR) {
			Return (API->error);
		}
	}
	if (Ctrl->A.active && !Ctrl->A.return_image) {
		gmt_M_str_free (to_GDALW->P.ProjectionRefPROJ4);
		gmt_M_str_free (to_GDALW->type);
		gmt_M_free (GMT, to_GDALW);
		gmt_M_str_free (Ctrl->A.driver);
		gmt_M_str_free (Ctrl->A.file);
	}
	if (!Ctrl->C.active && GMT_Destroy_Data (API, &P) != GMT_NOERROR) {
		Return (API->error);
	}
	Return (GMT_NOERROR);
}
