//=================================================================
// Code to create a table of tca-fit sets at various temperatures,
// and to create interpolated fit sets for arbitrary temperature.
//
// Stephen R. Whiteley, wrcad.com,  Synopsys, Inc.  2/25/2022
//=================================================================
// Released under Apache License, Version 2.0.
//   http://www.apache.org/licenses/LICENSE-2.0
//

#ifndef TSCALE_H
#define TSCALE_H

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>


// Structure to implement polynomial curve fitting.
//
struct mmjco_tscale
{
    mmjco_tscale(int nvals, const double *tvals, const double *fvals) :
        ts_tvals(tvals), ts_fvals(fvals), ts_nvals(nvals) { }

    // Return the interpolated f-value at the given t.
    //
    double value(double t)
        {
            double val = 0.0;
            for (int n = 0; n < ts_nvals; n++) {
                double prod = 1.0;
                for (int i = 0; i < ts_nvals; i++) {
                    if (i == n)
                        continue;
                    prod *= (t - ts_tvals[i])/(ts_tvals[n] - ts_tvals[i]);
                }
                val += prod*ts_fvals[n];
            }
            return (val);
        }

private:
    const double *ts_tvals;
    const double *ts_fvals;
    int ts_nvals;
};


// Structure representing a table of tca fit sets for different
// temperatures.  Other than temperature and gaps, the fit set
// parameters should match.
//
class mmjco_mtdb
{
public:
    mmjco_mtdb() :
        mt_nterms(0), mt_ntemps(0), mt_temps(0), mt_data(0), mt_names(0),
        mt_sm(0.0), mt_thr(0.0), mt_xp(0) { }

    ~mmjco_mtdb()
        {
            delete [] mt_temps;
            delete [] mt_data;
            if (mt_names) {
                for (int i = 0; i < mt_ntemps; i++)
                    delete [] mt_names[i];
                delete [] mt_names;
            }
        }

    // Create and initialize data areas.
    //
    bool setup(int ntemps, int nterms)
        {
            mt_nterms = nterms;
            mt_ntemps = ntemps;
            mt_temps = 0;
            mt_data = 0;
            mt_names = 0;
            if (ntemps < 2 || ntemps > 10)
                return (false);
            if (nterms < 6 || nterms > 16)
                return (false);
            mt_temps = new double[mt_ntemps];
            memset(mt_temps, 0, mt_ntemps*sizeof(double));
            mt_data = new double[mt_ntemps*mt_nterms*6];
            memset(mt_data, 0, mt_ntemps*mt_nterms*6*sizeof(double));
            mt_names = new const char*[mt_ntemps];
            memset(mt_names, 0, mt_ntemps*sizeof(const char*));
            return (true);
        }

    // Add a tca fit set with given name, temperature and data at
    // index ix.
    //
    bool add_table(const char *name, double t, int ix, double *data)
        {
            if (ix < 0 || ix >= mt_ntemps)
                return (false);
            mt_temps[ix] = t;
            double *dp = mt_data + ix*mt_nterms*6;
            memcpy(dp, data, mt_nterms*6*sizeof(double));
            delete [] mt_names[ix];
            mt_names[ix] = strdup(name ? name : "unknown");
            return (true);
        } 

    int num_terms() const   { return (mt_nterms); }
    int num_temps() const   { return (mt_ntemps); }
    int num_xp()    const   { return (mt_xp); }
    double smooth() const   { return (mt_sm); }
    double thresh() const   { return (mt_thr); }

    double *new_tab(double);
    bool load(FILE*);

private:
    int mt_nterms;
    int mt_ntemps;
    double *mt_temps;
    double *mt_data;
    const char **mt_names;
    double mt_sm;
    double mt_thr;
    int mt_xp;
};

#endif

