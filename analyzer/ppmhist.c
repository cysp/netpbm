/* ppmhist.c - read a PPM image and compute a color histogram
**
** Copyright (C) 1989 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include <assert.h>

#include "pm_c_util.h"
#include "mallocvar.h"
#include "nstring.h"
#include "shhopt.h"
#include "ppm.h"

enum sort {SORT_BY_FREQUENCY, SORT_BY_RGB};

enum colorFmt {FMT_DECIMAL, FMT_HEX, FMT_FLOAT, FMT_PPMPLAIN};

struct cmdline_info {
    /* All the information the user supplied in the command line,
       in a form easy for the program to use.
    */
    const char * inputFileName;  /* Name of input file */
    unsigned int noheader;    /* -noheader option */
    enum colorFmt colorFmt;
    unsigned int colorname;   /* -colorname option */
    enum sort sort;           /* -sort option */
};



static void
parseCommandLine(int argc, const char ** argv,
                 struct cmdline_info * const cmdlineP) {
/*----------------------------------------------------------------------------
   Note that the file spec array we return is stored in the storage that
   was passed to us as the argv array.
-----------------------------------------------------------------------------*/
    optStruct3 opt;  /* set by OPTENT3 */
    optEntry * option_def;
    unsigned int option_def_index;
    
    unsigned int hexcolorOpt, floatOpt, mapOpt, nomapOpt;
    const char * sort_type;

    MALLOCARRAY(option_def, 100);

    option_def_index = 0;   /* incremented by OPTENTRY */
    OPTENT3(0,   "map",       OPT_FLAG, NULL,  &mapOpt,                0);
    OPTENT3(0,   "nomap",     OPT_FLAG, NULL,  &nomapOpt,              0);
    OPTENT3(0,   "noheader",  OPT_FLAG, NULL,  &cmdlineP->noheader,    0);
    OPTENT3(0,   "hexcolor",  OPT_FLAG, NULL,  &hexcolorOpt,           0);
    OPTENT3(0,   "float",     OPT_FLAG, NULL,  &floatOpt,              0);
    OPTENT3(0,   "colorname", OPT_FLAG, NULL,  &cmdlineP->colorname,   0);
    OPTENT3(0,   "sort",      OPT_STRING, &sort_type, NULL,            0);

    opt.opt_table = option_def;
    opt.short_allowed = FALSE;  /* We have no short (old-fashioned) options */
    opt.allowNegNum = FALSE;  /* We have no parms that are negative numbers */

    /* Set defaults */
    sort_type = "frequency";

    pm_optParseOptions3(&argc, (char **)argv, opt, sizeof(opt), 0);
        /* Uses and sets argc, argv, and some of *cmdlineP and others. */

    if (argc-1 == 0) 
        cmdlineP->inputFileName = "-";
    else if (argc-1 != 1)
        pm_error("Program takes zero or one argument (filename).  You "
                 "specified %d", argc-1);
    else
        cmdlineP->inputFileName = argv[1];

    if (hexcolorOpt + floatOpt + mapOpt > 1)
        pm_error("You can specify only one of -hexcolor, -float, and -map");
    if (hexcolorOpt)
        cmdlineP->colorFmt = FMT_HEX;
    else if (floatOpt)
        cmdlineP->colorFmt = FMT_FLOAT;
    else if (mapOpt)
        cmdlineP->colorFmt = FMT_PPMPLAIN;
    else 
        cmdlineP->colorFmt = FMT_DECIMAL;

    if (strcmp(sort_type, "frequency") == 0)
        cmdlineP->sort = SORT_BY_FREQUENCY;
    else if (strcmp(sort_type, "rgb") == 0)
        cmdlineP->sort = SORT_BY_RGB;
    else
        pm_error("Invalid -sort value: '%s'.  The valid values are "
                 "'frequency' and 'rgb'.", sort_type);
}



static int
cmpUint(unsigned int const a,
        unsigned int const b) {
/*----------------------------------------------------------------------------
   Return 1 if 'a' > 'b'; -1 if 'a' is < 'b'; 0 if 'a' == 'b'.

   I.e. what a libc qsort() comparison function returns.
-----------------------------------------------------------------------------*/
    return a > b ? 1 : a < b ? -1 : 0;
}



#ifndef LITERAL_FN_DEF_MATCH
static qsort_comparison_fn countcompare;
#endif


static int
countcompare(const void * const a,
             const void * const b) {
/*----------------------------------------------------------------------------
   This is a 'qsort' collation function.
-----------------------------------------------------------------------------*/
    const struct colorhist_item * const histItem1P = a;
    const struct colorhist_item * const histItem2P = b;

    return cmpUint(histItem2P->value, histItem1P->value);
}



#ifndef LITERAL_FN_DEF_MATCH
static qsort_comparison_fn rgbcompare;
#endif

static int
rgbcompare(const void * const a,
           const void * const b) {
/*----------------------------------------------------------------------------
   This is a 'qsort' collation function.
-----------------------------------------------------------------------------*/
    const struct colorhist_item * const histItem1P = a;
    const struct colorhist_item * const histItem2P = b;

    int retval;

    retval = cmpUint(PPM_GETR(histItem1P->color),
                     PPM_GETR(histItem2P->color));
    if (retval == 0) {
        retval = cmpUint(PPM_GETG(histItem1P->color),
                         PPM_GETG(histItem2P->color));
        if (retval == 0)
            retval = cmpUint(PPM_GETB(histItem1P->color),
                             PPM_GETB(histItem2P->color));
    }
    return retval;
}



static const char *
colornameLabel(pixel        const color, 
               pixval       const maxval,
               unsigned int const nDictColor,
               pixel        const dictColors[],
               const char * const dictColornames[]) {
/*----------------------------------------------------------------------------
   Return the name of the color 'color' or the closest color in the
   dictionary to it.  If the name returned is not the exact color,
   prefix it with "*".  Otherwise, prefix it with " ".

   'nDictColor', dictColors[], and dictColorNames[] are the color 
   dictionary.

   Return the name in static storage within this subroutine.
-----------------------------------------------------------------------------*/
    static char retval[32];
    int colorIndex;
    
    pixel color255;  
        /* The color, normalized to a maxval of 255: the maxval of a color
           dictionary.
        */

    PPM_DEPTH(color255, color, maxval, 255);

    colorIndex = ppm_findclosestcolor(dictColors, nDictColor, &color);

    assert(colorIndex >= 0 && colorIndex < nDictColor);
    
    if (PPM_EQUAL(dictColors[colorIndex], color))
        STRSCPY(retval, " ");
    else
        STRSCPY(retval, "*");
    
    STRSCAT(retval, dictColornames[colorIndex]);
    
    return retval;
}
                


static void
printColors(colorhist_vector const chv, 
            int              const nColors,
            pixval           const maxval,
            enum colorFmt    const colorFmt,
            unsigned int     const nKnown,
            pixel            const knownColors[],
            const char *     const colornames[]) {

    int i;

    for (i = 0; i < nColors; i++) {
        pixval       const r          = PPM_GETR(chv[i].color);
        pixval       const g          = PPM_GETG(chv[i].color);
        pixval       const b          = PPM_GETB(chv[i].color);
        double       const lum        = PPM_LUMIN(chv[i].color);
        unsigned int const intLum     = lum + 0.5;
        double       const floatLum   = lum / maxval;
        unsigned int const count      = chv[i].value;

        const char * colornameValue;

        if (colornames)
            colornameValue = colornameLabel(chv[i].color, maxval, 
                                            nKnown, knownColors, colornames);
        else
            colornameValue = "";

        switch(colorFmt) {
        case FMT_FLOAT:
            printf(" %1.3f %1.3f %1.3f\t%1.3f\t%7d %s\n",
                   (double)r / maxval,
                   (double)g / maxval,
                   (double)b / maxval,
                   floatLum, count, colornameValue);
            break;
        case FMT_HEX:
            printf("  %04x  %04x  %04x\t%5d\t%7d %s\n",
                   r, g, b, intLum, count, colornameValue);
            break;
        case FMT_DECIMAL:
            printf(" %5d %5d %5d\t%5d\t%7d %s\n",
                   r, g, b, intLum, count, colornameValue);
            break;
        case FMT_PPMPLAIN:
            printf(" %5d %5d %5d#\t%5d\t%7d %s\n",
                   r, g, b, intLum, count, colornameValue);
            break;
        }
    }
}



int
main(int argc, const char *argv[]) {

    struct cmdline_info cmdline;
    FILE * ifP;
    colorhist_vector chv;
    int rows, cols;
    pixval maxval;
    int format;
    int nColors;
    int (*compare_function)(const void *, const void *);
        /* The compare function to be used with qsort() to sort the
           histogram for output
        */
    unsigned int nDictColor;
    const char ** dictColornames;
    pixel * dictColors;

    pm_proginit(&argc, argv);

    parseCommandLine(argc, argv, &cmdline);

    ifP = pm_openr(cmdline.inputFileName);

    ppm_readppminit(ifP, &cols, &rows, &maxval, &format);

    chv = ppm_computecolorhist2(ifP, cols, rows, maxval, format, 0, &nColors);

    pm_close(ifP);

    switch (cmdline.sort) {
    case SORT_BY_FREQUENCY:
        compare_function = countcompare; break;
    case SORT_BY_RGB:
        compare_function = rgbcompare; break;
    }

    qsort((char*) chv, nColors, sizeof(struct colorhist_item), 
          compare_function);

    /* And print the histogram. */
    if (cmdline.colorFmt == FMT_PPMPLAIN) 
        printf("P3\n# color map\n%d 1\n%d\n", nColors, maxval);

    if (!cmdline.noheader) {
        const char commentDelim = cmdline.colorFmt == FMT_PPMPLAIN ? '#' : ' ';
        printf("%c  r     g     b   \t lum \t count  %s\n",
               commentDelim, cmdline.colorname ? "name" : "");
        printf("%c----- ----- ----- \t-----\t------- %s\n",
               commentDelim, cmdline.colorname ? "----" : "");
    }
    if (cmdline.colorname) {
        bool mustOpenTrue = TRUE;
        ppm_readcolordict(NULL, mustOpenTrue, 
                          &nDictColor, &dictColornames, &dictColors, NULL);
    } else {
        dictColors = NULL;
        dictColornames = NULL;
    }
        
    printColors(chv, nColors, maxval,
                cmdline.colorFmt, nDictColor, dictColors, dictColornames);

    if (dictColors)
        free(dictColors);
    if (dictColornames)
        free(dictColornames);

    ppm_freecolorhist(chv);

    return 0;
}
