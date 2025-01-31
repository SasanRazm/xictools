//==============================================
// Command structure implementations for mmjco.
// S. R. Whiteley, wrcad.com,  Synopsys, Inc
//==============================================
//
// License:  GNU General Public License Version 3m 29 June 2007.

#include "mmjco_cmds.h"
#include "mmjco_tempr.h"
#include "mmjco_tscale.h"
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#ifdef WRSPICE
#include "../../xt_base/include/miscutil/pathlist.h"
// This brings in lstring.h.
#endif


namespace {
    void gslhdlr(const char*, const char*, int, int)
    {
    }

    // Return true if the string represents an absolute path.
    //
    bool
    is_rooted(const char *string)
    {
        if (!string || !*string)
            return (false);
        if (*string == '/')
            return (true);
        if (*string == '~')
            return (true);
        if (*string == '.')
            return (true);
#ifdef WIN32
        if (*string == '\\')
            return (true);
        if (strlen(string) >= 3 && isalpha(string[0]) && string[1] == ':' &&
                (string[2] == '/' || string[2] == '\\'))
            return (true);
#endif
        return (false);
    }
}


mmjco_cmds::mmjco_cmds()
{
    mmc_pair_data   = 0;
    mmc_qp_data     = 0;
    mmc_xpts        = 0;
    mmc_numxpts     = 0;
#ifdef WRSPICE
    // Use real-valued rawfile, WaveView is not friendly with
    // complex.
    mmc_ftype = DFRAWREAL;
#else
    mmc_ftype = DFDATA;
#endif
    mmc_temp        = 0.0;
    mmc_d1          = 0.0;
    mmc_d2          = 0.0;
    mmc_sm          = 0.0;
    mmc_tc1         = 0.0;
    mmc_tc2         = 0.0;
    mmc_td1         = 0.0;
    mmc_td2         = 0.0;
    mmc_datafile    = 0;
    mmc_tcadir      = 0;

    mmc_nterms      = 0;
    mmc_thr         = 0.0;
    mmc_fitfile     = 0;

    mmc_pair_model  = 0;
    mmc_qp_model    = 0;

    gsl_set_error_handler(&gslhdlr);

#ifdef WRSPICE
    char *home = pathlist::get_home();
    if (home) {
        // Set default TCA directory to $HOME/.mmjco.
        sLstr lstr;
        lstr.add(home);
        delete [] home;
        lstr.add("/.mmjco");
        char *av[2];
        av[1] = lstr.string_trim();
        mm_set_dir(2, av);
        delete [] av[1];
    }
#endif
}


mmjco_cmds::~mmjco_cmds()
{
    delete [] mmc_pair_data;
    delete [] mmc_qp_data;
    delete [] mmc_xpts;
    delete [] mmc_datafile;
    delete [] mmc_tcadir;
    delete [] mmc_fitfile;
    delete [] mmc_pair_model;
    delete [] mmc_qp_model;
}


// Set the directory path for default input/output of TCA files.
//
int
mmjco_cmds::mm_set_dir(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Error: missing directory path.\n");
        return (1);
    }
    const char *dir = argv[1];
    struct stat st;
    if (stat(dir, &st) < 0) {
#ifdef WIN32
        mkdir(dir);
#else
        mkdir(dir, 0777);
#endif
        if (stat(dir, &st) < 0) {
            fprintf(stderr, "Error: cannot stat path.\n");
            return (1);
        }
    }
    if (!(st.st_mode & S_IFDIR)) {
        fprintf(stderr, "Error: path is not a directory.\n");
        return (1);
    }
    delete [] mmc_tcadir;
    char *bf = new char[strlen(dir) + 1];
    strcpy(bf, dir);
    mmc_tcadir = bf;
    return (0);
}


int
mmjco_cmds::mm_get_gap(int argc, char **argv)
{
    double tc = TC_NB;
    double td = DEBYE_TEMP_NB;
    double temps[10];
    int ix = 0;

    for (int i = 1; i < argc; i++) {
        double a;
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-tc")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing transition temperature.\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a >= 0.0 && a < 300.0)
                    tc = a;
                else {
                    fprintf(stderr, "Error: bad -tc (temperature).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-td")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing Debye temperature.\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a >= 0.0)
                    tc = a;
                else {
                    fprintf(stderr, "Error: bad -td (temperature).\n");
                    return (1);
                }
                continue;
            }
        }
        if (sscanf(argv[i], "%lf", &a) == 1 && a >= 0.0 && a < 300.0) {
            if (ix < 10)
                temps[ix++] = a;
        }
    }

    mmjco_tempr tp(tc, td);
    if (ix == 0) {
        for (double T = 0.0; T <= tc; T += 0.1) {
            double del = tp.gap_parameter(T);
#define CHECK_FIT_FUNC
#ifdef CHECK_FIT_FUNC
            double fit = 1.3994e-3*tanh(1.74*sqrt(tc/(T+1e-3) - 1.0));
            double pcterr = 100*2*fabs(del - fit)/(del + fit);
            printf("T= %.4e Del= %.4e Fit= %.4e PctErr= %.4e\n", T, del,
                fit, pcterr);
#else
            printf("T= %.4e Del= %.4e\n", T, del);
#endif
        }
    }
    else if (ix == 2) {
        double Tmin = temps[0];
        double Tmax = temps[1];
        if (Tmin >= 0.0 && Tmin <= tc && Tmax >= 0.0 && Tmax <= tc) {
            if (Tmin > Tmax) {
                double tmp = Tmin;
                Tmin = Tmax;
                Tmax = tmp;
            }
            for (double T = Tmin; T <= Tmax; T += 0.1) {
                double del = tp.gap_parameter(T);
                printf("T= %.4e Del= %.4e\n", T, del);
            }
        }
    }
    else {
        for (int i = 0; i < ix; i++) {
            if (temps[i] >= 0.0 && temps[i] <= tc) {
                double del = tp.gap_parameter(temps[i]);
                printf("T= %.4e Del= %.4e\n", temps[i], del);
            }
        }
    }
    return (0);
}


// Return the possibly interpolated fit data for temp from the sweep
// file.
//
int
mmjco_cmds::mm_get_sweep_fit(int argc, char **argv, mmjco_mtdb **pmt,
    double *pt, char **pfn)
{
    if (pmt)
        *pmt = 0;
    if (pt)
        *pt = 0.0;
    if (pfn)
        *pfn = 0;
    char *swpfile = 0;
    double temp = -1.0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-fs")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing sweep filename.\n");
                    return (1);
                }
                char f[256];
                if (sscanf(argv[i], "%s", f) == 1) {
                    if (mmc_tcadir && !is_rooted(f)) {
                        swpfile = new char[strlen(mmc_tcadir) + strlen(f) +2];
                        sprintf(swpfile, "%s/%s", mmc_tcadir, f);
                    }
                    else {
                        swpfile = new char[strlen(f)+1];
                        strcpy(swpfile, f);
                    }
                }
                else {
                    fprintf(stderr, "Error: bad -fs (sweep filename).\n");
                    return (1);
                }
                continue;
            }
        }
        double a;
        if (sscanf(argv[i], "%lf", &a) == 1 && a >= 0.0 && a < 300.0) {
            temp = a;
        }
        else {
            fprintf(stderr, "Error: bad temperature.\n");
            return (1);
        }
    }
    if (temp == -1.0) {
        fprintf(stderr, "Error: temperature not given.\n");
        return (1);
    }
    if (!swpfile) {
        fprintf(stderr, "Error: sweep file not given.\n");
        return (1);
    }
    FILE *fp = fopen(swpfile, "r");
    if (!fp) {
        fprintf(stderr, "Error: can not open %s.\n", swpfile);
        delete [] swpfile;
        return (1);
    }

    int nrec;
    double tstrt, tdel;
    char *tbuf = new char[128];
    if (!fgets(tbuf, 64, fp)) {
        fprintf(stderr, "Error: premature EOF in %s.\n", swpfile);
        delete [] tbuf;
        delete [] swpfile;
        return (1);
    }

    double sm, th;
    int xp, nterms;
    if (strncmp(tbuf, "tsweep", 6) || sscanf(tbuf+7, "%d %lf %lf %lf %d %d %lf",
            &nrec, &tstrt, &tdel, &sm, &xp, &nterms, &th) != 7) {
        fprintf(stderr, "Error: syntax in %s.\n", swpfile);
        delete [] tbuf;
        delete [] swpfile;
        return (1);
    }
    delete [] tbuf;
    mmc_sm = sm;
    mmc_thr = th;
    mmc_numxpts = xp;

    int ntemp = (temp - tstrt)/tdel + 1e-6;
    if (ntemp < 0 || ntemp >= nrec) {
        fprintf(stderr, "Error: range mismatch in %s.\n", swpfile);
        delete [] swpfile;
        return (1);
    }
    double resid = (temp - tstrt) - ntemp*tdel;
    // On-point, linear, or quadratic.
    int recs = 0;
    if (resid < 1e-6) {
        // Close enough to reference, no interpolation needed.
        recs = 1;
    }
    else {
        recs = 2;
        /* Quadratic interpolation doesn't seem to give good results.
        if (resid > 0.5) {
            // Closer to the next reference, do quad unless at upper limit.
            if (ntemp + 2 < nrec)
                recs = 3;
        }
        else if (ntemp > 0) {
            // Do quad unless at lower limit.
            ntemp--;
            recs = 3;
        }
        */
    }

    // Compute the record size, these should all be the same.
    long pos1 = ftell(fp);
    struct stat st;
    fstat(fileno(fp), &st);
    long sz = st.st_size - pos1;
    long recsz = sz/nrec;

    // Set the offset into the file.
    long offset = recsz*ntemp;
    if (fseek(fp, offset, SEEK_CUR) < 0) {
        fprintf(stderr, "Error: seek failed in %s.\n", swpfile);
        delete [] swpfile;
        return (1);
    }

    // Grab the data.
    mmjco_mtdb *mt = new mmjco_mtdb;
    if (!mt->load(fp, recs, nterms)) {
        fprintf(stderr, "Error: load failed.\n");
        delete mt;
        delete [] swpfile;
        fclose(fp);
        return (1);
    }
    if (pt)
        *pt = temp;
    if (pmt) {
        mt->set_temp(temp, mmc_numxpts, mmc_sm, mmc_thr);
        *pmt = mt;
    }
    else
        delete mt;
    if (pfn)
        *pfn = swpfile;
    else
        delete [] swpfile;

    fclose(fp);
    return (0);
}


// Create a TCA data set and write this to a file.  The data set is
// saved in an internal data register, replacing any existing data.
// The default temp can be passed in the args for sweeps.
//
// cd[ata] [-t temp] [-tc1|-tc2|-tc Tc] [-td1|-td2|-td Tdebye]
//  [-d|-d1|-d2 delta] [-s smooth] [-x nx] -f [filename] [-r | -rr | -rd]
//
int
mmjco_cmds::mm_create_data(int argc, char **argv, double temp, bool no_out)
{
    double tc1 = TC_NB;
    double tc2 = TC_NB;
    double td1 = DEBYE_TEMP_NB;
    double td2 = DEBYE_TEMP_NB;
    double d1 = 0.0;
    double d2 = 0.0;
    double sm = 0.008;
    int nx  = 500;
    char *datafile = 0;
    DFTYPE dtype = mmc_ftype;
    int nterms = 8;
    double thr = 0.2;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            double a;
            if (!strcmp(argv[i], "-t")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing temperature.\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a >= 0.0 && a < 300.0)
                    temp = a;
                else {
                    fprintf(stderr, "Error: bad -t (temperature).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-tc1")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -tc1 (temperature).\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a > 0.0 && a < 300.0) {
                    tc1 = a;
                }
                else {
                    fprintf(stderr, "Error: bad -tc1 (temperature).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-tc2")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -tc2 (temperature).\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a > 0.0 && a < 300.0) {
                    tc2 = a;
                }
                else {
                    fprintf(stderr, "Error: bad -tc2 (temperature).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-tc")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -tc (temperature).\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a > 0.0 && a < 300.0) {
                    tc1 = a;
                    tc2 = a;
                }
                else {
                    fprintf(stderr, "Error: bad -tc (temperature).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-td1")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -td1 (temperature).\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a > 0.0 && a < 1000.0) {
                    td1 = a;
                }
                else {
                    fprintf(stderr, "Error: bad -td1 (temperature).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-td2")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -td2 (temperature).\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a > 0.0 && a < 1000.0) {
                    td2 = a;
                }
                else {
                    fprintf(stderr, "Error: bad -td2 (temperature).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-td")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -td (temperature).\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a > 0.0 && a < 1000.0) {
                    td1 = a;
                    td2 = a;
                }
                else {
                    fprintf(stderr, "Error: bad -td (temperature).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-d1")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -d1 (delta V).\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a > 0.0 && a < 5.0) {
                    d1 = a;
                }
                else {
                    fprintf(stderr, "Error: bad -d1 (delta V).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-d2")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -d2 (delta V).\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a > 0.0 && a < 5.0) {
                    d2 = a;
                }
                else {
                    fprintf(stderr, "Error: bad -d2 (delta V).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-d")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -d (delta V).\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a > 0.0 && a < 5.0) {
                    d1 = a;
                    d2 = a;
                }
                else {
                    fprintf(stderr, "Error: bad -d (delta V).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-s")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -s (smoothing factor).\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a >= 0.0 && a < 0.099)
                    sm = (a >= 0.001 ? a : 0.0);
                else {
                    fprintf(stderr,
                        "Error: bad -s (smoothing factor).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-x")) {
                if (++i == argc) {
                    fprintf(stderr,
                        "Error: missing -x (number of datapoints).\n");
                    return (1);
                }
                int d;
                if (sscanf(argv[i], "%d", &d) == 1 && d >= 10 && d <= 10000)
                    nx = d;
                else {
                    fprintf(stderr,
                        "Error: bad -x (number of datapoints).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-f")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -f (data filename).\n");
                    return (1);
                }
                char f[256];
                if (sscanf(argv[i], "%s", f) == 1)
                    datafile = strdup(f);
                else {
                    fprintf(stderr, "Error: bad -f (data filename).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-rr")) {
                dtype = DFRAWREAL;
                continue;
            }
            if (!strcmp(argv[i], "-rd")) {
                dtype = DFDATA;
                continue;
            }
            if (!strcmp(argv[i], "-r")) {
                dtype = DFRAWCPLX;
                continue;
            }

            // If no_out is true, we are generating a sweep table, and
            // need the values for the following fit parameters to be
            // set, as they are used to create the sweep file name. 
            // These will be set again when creating fit data.

            if (!strcmp(argv[i], "-n")) {
                if (!no_out)
                    continue;
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -n (term count).\n");
                    return (1);
                }
                int d;
                if (sscanf(argv[i], "%d", &d) == 1 && d > 3 && d < 21 && !(d&1))
                    nterms = d;
                else {
                    fprintf(stderr,
                        "Error: bad -n (term count, expect 4-20 even).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-h")) {
                if (!no_out)
                    continue;
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -h (threshold).\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a >= 0.05 && a < 0.5)
                    thr = a;
                else {
                    fprintf(stderr, "Error: bad -h (threshold).\n");
                    return (1);
                }
                continue;
            }
        }
    }
    if (temp > (tc1 + tc2)*0.999) {
        fprintf(stderr, "Error: temperature close to or above Tc.\n");
        return (1);
    }
    if (no_out) {
        mmc_nterms = nterms;
        mmc_thr = thr;
    }

    // Setup TCA computation.
    if (d1 == 0.0) {
        mmjco_tempr tmpr(tc1, td1);
        d1 = tmpr.gap_parameter(temp) * 1e3;
    }
    if (d2 == 0.0) {
        mmjco_tempr tmpr(tc2, td2);
        d2 = tmpr.gap_parameter(temp) * 1e3;
    }

    mmjco m(temp, d1, d2, sm);

    delete [] mmc_pair_data;
    delete [] mmc_qp_data;
    delete [] mmc_xpts;
    mmc_temp = temp;
    mmc_d1 = d1*1e-3;
    mmc_d2 = d2*1e-3;
    mmc_sm = sm;
    mmc_tc1 = tc1;
    mmc_tc2 = tc2;
    mmc_td1 = td1;
    mmc_td2 = td2;
    mmc_numxpts = nx;
    mmc_pair_data = new complex<double>[mmc_numxpts];
    mmc_qp_data = new complex<double>[mmc_numxpts];
    mmc_xpts = new double[mmc_numxpts];

    // Compute TCA data.
    double x0 = 0.001;
    double dx = (2.0 - x0)/(mmc_numxpts-1);
    for (int i = 0; i < mmc_numxpts; i++) {
        double xx = x0;
        // Need to avoid xx == 1.0 which causes integration failure.
        if (fabs(xx - 1.0) < 1e-5)
            xx = 1.0 + 1e-5;
        if (sm > 0.0) {
            mmc_pair_data[i] = m.Jpair_smooth(xx);
            mmc_qp_data[i] = m.Jqp_smooth(xx);
        }
        else {
            mmc_pair_data[i] = m.Jpair(xx);
            mmc_qp_data[i] = m.Jqp(xx);
        }
        mmc_xpts[i] = xx;
        x0 += dx;
    }

    if (no_out && datafile) {
        delete [] datafile;
        datafile = 0;
    }
    const char *filename = datafile;
    bool df_given = true;
    if (!datafile) {
        df_given = false;
        char *dp;
        if (mmc_tcadir) {
            datafile = new char[strlen(mmc_tcadir) + 32];
            sprintf(datafile, "%s/", mmc_tcadir);
            dp = datafile + strlen(datafile);
        }
        else {
            datafile = new char[32];
            dp = datafile;
        }
        filename = dp;
        sprintf(dp, "tca%06ld%05ld%05ld%02ld%04d",
            lround(mmc_temp*1e4), lround(mmc_d1*1e7), lround(mmc_d2*1e7),
            lround(mmc_sm*1e3), mmc_numxpts);
        if (dtype == DFDATA)
            strcat(dp, ".data");
        else
            strcat(dp, ".raw");
    }
    if (!no_out) {
        if (df_given) {
            if (mmc_tcadir && !is_rooted(datafile)) {
                char *t = new char[strlen(datafile) + strlen(mmc_tcadir) + 2];
                sprintf(t, "%s/%s", mmc_tcadir, datafile);
                delete [] datafile;
                datafile = t;
                filename = datafile + strlen(mmc_tcadir)+1;
            }
        }

        FILE *fp = fopen(datafile, "w");
        if (!fp) {
            fprintf(stderr, "Error: unable to write file %s.\n", filename);
            delete [] datafile;
            return (1);
        }
        save_data(filename, fp, dtype, mmc_xpts, mmc_pair_data, mmc_qp_data,
            mmc_numxpts);
        fclose(fp);
    }
    delete [] mmc_datafile;
    mmc_datafile = datafile;
    return (0);
}


// Create a fitting table for the existing internal TCA data and write
// this to a file.  The fitting paramers are stored internally.  The
// destination can be coerced by passing fp.
//
// cf[it] [-n terms] [-h thr] [-ff filename]
//
int
mmjco_cmds::mm_create_fit(int argc, char **argv, FILE *fp)
{
    if (!mmc_numxpts) {
        fprintf(stderr,
            "Error: no TCA data in memory, use \"cd\" or \"ld\".\n");
        return (1);
    }
    int nterms = 8;
    double thr = 0.2;
    char *fitfile = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            double a;
            if (!strcmp(argv[i], "-n")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -n (term count).\n");
                    return (1);
                }
                int d;
                if (sscanf(argv[i], "%d", &d) == 1 && d > 3 && d < 21 && !(d&1))
                    nterms = d;
                else {
                    fprintf(stderr,
                    "Error: bad -n (term count, expect 4-20 even).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-h")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -h (threshold).\n");
                    return (1);
                }
                if (sscanf(argv[i], "%lf", &a) == 1 && a >= 0.05 && a < 0.5)
                    thr = a;
                else {
                    fprintf(stderr, "Error: bad -h (threshold).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-ff")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -ff (fit filename).\n");
                    return (1);
                }
                char f[256];
                if (sscanf(argv[i], "%s", f) == 1)
                    fitfile = strdup(f);
                else {
                    fprintf(stderr, "Error: bad -ff (fit filename).\n");
                    return (1);
                }
                continue;
            }
        }
    }

    mmc_nterms = nterms;
    mmc_thr = thr;

    // Compute the sub-gap resistance parameter.

    complex<double> *qp = mmc_qp_data;
    double dx = 1.999/(mmc_numxpts - 1);

    // Index of the point closest to 80% of Vgap.  Also index a point
    // just above the gap.  This is used to "calibrate" the y-axis.
    int np8 = round((0.8 - 0.001)/dx);
    int nu = round((1.015 - 0.001)/dx);

    double yp8 = qp[np8].imag();
    double yu = qp[nu].imag();
    double ip8 = yp8/(yu - yp8);

    // Example: Compute the approximate intrinsic subgap resistance.
    // double fct = 0.8*(TJMdel1Nom + TJMdel2Nom)*(TJMicFactor/TJMcriti);
    // double rsint = fct/ip8;

    // Compute fitting parameters.
    printf("Computing fitting parameters at T=%.4fK\n", mmc_temp);
    mmc_mf.new_fit_parameters(mmc_xpts, mmc_pair_data, mmc_qp_data, mmc_numxpts,
        mmc_nterms, mmc_thr);

    if (!fitfile) {
        char *tbuf;
        if (mmc_datafile) {
            tbuf = new char[strlen(mmc_datafile) + 12];
            strcpy(tbuf, mmc_datafile);
            char *t = strrchr(tbuf, '.');
            if (t && t > tbuf)
                *t = 0;
        }
        else {
            if (mmc_tcadir) {
                tbuf = new char[strlen(mmc_tcadir) + 20];
                sprintf(tbuf, "%s/", mmc_tcadir);
            }
            else {
                tbuf = new char[20];
                *tbuf = 0;
            }
            strcat(tbuf, "tca_data");
        }
        
        sprintf(tbuf+strlen(tbuf), "-%02d%03ld.fit", mmc_nterms,
            lround(mmc_thr*1e3));
        fitfile = tbuf;
    }
    else if (mmc_tcadir && !is_rooted(fitfile)) {
        char *t = new char[strlen(mmc_tcadir) + strlen(fitfile) + 2];
        sprintf(t, "%s/%s", mmc_tcadir, fitfile);
        delete [] fitfile;
        fitfile = t;
    }

    // Print a header string.
    char *hdr = new char[80];
    if (fp) {
        // Sweep or table file.
        sprintf(hdr, "tcafit %11.4e %11.4e %11.4e %11.4e", mmc_temp, mmc_d1,
            mmc_d2, ip8);
    }
    else {
        // Stand-alone file.
        sprintf(hdr, "tcafit %11.4e %11.4e %11.4e %.3f %-4d %-2d %.3f %11.4e",
            mmc_temp, mmc_d1, mmc_d2, mmc_sm, mmc_numxpts, mmc_nterms, mmc_thr,
            ip8);
    }
    mmc_mf.save_fit_parameters(fitfile, fp, hdr);
    delete [] hdr;
    delete [] mmc_fitfile;
    mmc_fitfile = fitfile;
    return (0);
}


// Back-convert a "model" TCA parameter table from the fitting
// parameters stored in memory.  By default, output is not saved, only
// the reisdual to the original TCA table (if present) is computed. 
// Use -fm with no argument to save with default file name,
//
// cm [-h thr] [-fm [filename]] [-r | -rr | -rd]
int
mmjco_cmds::mm_create_model(int argc, char **argv)
{
    if (!mmc_numxpts) {
        fprintf(stderr,
            "Error: no TCA data in memory, use \"cd\" or \"ld\".\n");
        return (1);
    }
    if (mmc_nterms <= 0) {
        fprintf(stderr,
            "Error: no fit data in memory, use \"cf\" or \"lf\".\n");
        return (1);
    }

    DFTYPE dtype = mmc_ftype;
    double loc_thr = (mmc_thr > 0.0 ? mmc_thr : 0.2);
    bool got_f = false;
    char *modfile = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (!strcmp(argv[i], "-h")) {
                if (++i == argc) {
                    fprintf(stderr, "Error: missing -h (threshold).\n");
                    return (1);
                }
                double a;
                if (sscanf(argv[i], "%lf", &a) == 1 && a >= 0.05 && a < 0.5)
                    loc_thr = a;
                else {
                    fprintf(stderr, "Error: bad -h (threshold).\n");
                    return (1);
                }
                continue;
            }
            if (!strcmp(argv[i], "-fm")) {
                got_f = true;
                if (++i < argc) {
                    char f[256];
                    if (sscanf(argv[i], "%s", f) == 1)
                        modfile = strdup(f);
                    else {
                        fprintf(stderr, "Error: bad -fm (model filename).\n");
                        return (1);
                    }
                }
                continue;
            }
            if (!strcmp(argv[i], "-rr")) {
                dtype = DFRAWREAL;
                continue;
            }
            if (!strcmp(argv[i], "-rd")) {
                dtype = DFDATA;
                continue;
            }
            if (!strcmp(argv[i], "-r")) {
                dtype = DFRAWCPLX;
                continue;
            }
        }
    }

    delete [] mmc_pair_model;
    delete [] mmc_qp_model;
    mmc_mf.tca_fit(mmc_xpts, mmc_numxpts, &mmc_pair_model, &mmc_qp_model);

    if (mmc_pair_data && mmc_qp_data) {
        double res = mmc_mf.residual(mmc_pair_model, mmc_qp_model,
            mmc_pair_data, mmc_qp_data, mmc_numxpts, loc_thr);
        printf("Model created, residual = %g\n", res);
    }

    if (got_f) {
        if (!modfile) {
            char *tbuf;
            char *fn;
            if (mmc_fitfile) {
                tbuf = new char[strlen(mmc_fitfile) + 10];
                strcpy(tbuf, mmc_fitfile);
                char *t = strrchr(tbuf, '.');
                if (t && t > tbuf)
                    *t = 0;
                fn = tbuf + strlen(tbuf);
            }
            else {
                tbuf = new char[20];
                strcpy(tbuf, "tca_model");
                fn = tbuf;
            }
            if (dtype == DFDATA)
                strcat(fn, ".data");
            else
                strcat(fn, ".raw");
            modfile = tbuf;
        }
        // Note that these do not use mmc_tcadir.
        FILE *fp = fopen(modfile, "w");
        if (!fp) {
            fprintf(stderr, "Error: unable to open file %s.\n", modfile);
            delete [] modfile;
            return (1);
        }
        save_data(modfile, fp, dtype, mmc_xpts, mmc_pair_model, mmc_qp_model,
            mmc_numxpts);
        fclose(fp);
        delete [] modfile;
    }
    return (0);
}


// Create a sweep file which consists of fit parameters for
// temperatures over a range.  The dT defaults to 0.1K.
//
// cs[weep] T1 T2 [dT]  [ cd and cf args]
//
int
mmjco_cmds::mm_create_sweep(int argc, char **argv)
{
    int nvals = 0;
    for (int i = 1; i < argc; i++) {
        if ((!isdigit(*argv[i]) && *argv[i] != '.')) {
            nvals = i-1;
            break;
        }
        if (i == argc-1) {
            nvals = i;
            break;
        }
    }
    if (nvals < 2 || nvals > 3) {
        fprintf(stderr, "Error: 2 temperatures and optional delta required\n");
        return (1);
    }
    double *vals = new double[3];
    for (int i = 0; i < nvals; i++) {
        vals[i] = atof(argv[i+1]);
        if (vals[i] <= 0) {
            fprintf(stderr, "Error: non-positive temperature or delta\n");
            return (1);
        }
    }
    if (vals[0] > vals[1]) {
        double t = vals[0];
        vals[0] = vals[1];
        vals[1] = t;
    }
    if (nvals == 2)
        vals[2] = 0.1;
    int ntemps = (vals[1] - vals[0])/vals[2] + 1.001;

    char *datafile;
    char *dp;
    if (mmc_tcadir) {
        datafile = new char[strlen(mmc_tcadir) + 40];
        sprintf(datafile, "%s/", mmc_tcadir);
        dp = datafile + strlen(datafile);
    }
    else {
        datafile = new char[40];
        dp = datafile;
    }

    int err = 0;
    FILE *fp = 0;
    bool done_header = false;
    for (int i = 0; i < ntemps; i++) {
        if (mm_create_data(argc-nvals, argv+nvals, vals[0], true)) {
            err = 1;
            break;
        }
        if (!done_header) {
            done_header = true;
            if (vals[0] + ntemps*vals[2] > (mmc_tc1 + mmc_tc2)*0.999) {
                fprintf(stderr, "Error: upper range close to or above Tc.\n");
                err = 1;
                break;
            }
            sprintf(dp, "tsw%03d%06ld%06ld%02ld%04d-%02d%03ld.swp", ntemps,
                lround(vals[0]*1e4), lround(vals[2]*1e4), lround(mmc_sm*1e3),
                mmc_numxpts, mmc_nterms, lround(mmc_thr*1e3));
            fp = fopen(datafile, "w");
            if (!fp) {
                fprintf(stderr, "Error: failed to open %s for writing.\n",
                    datafile);
                err = 1;
                break;
            }
            fprintf(fp, "tsweep %d %.4f %.4f %.3f %d %d %.3f\n", ntemps, vals[0],
                vals[2], mmc_sm, mmc_numxpts, mmc_nterms, mmc_thr);
        }

        if (mm_create_fit(argc-nvals, argv+nvals, fp)) {
            err = 1;
            break;
        }
        vals[0] += vals[2];
    }
    if (fp)
        fclose(fp);
    delete [] vals;
    delete [] datafile;
    return (err);
}


// Create a tab file which consists of fit data for each of the
// temperatures listed.
//
// ct[ab] T1 T2 [... Tn] [ cd and cf args]
//
int
mmjco_cmds::mm_create_table(int argc, char **argv)
{
    int ntemps = 0;
    for (int i = 1; i < argc; i++) {
        if ((!isdigit(*argv[i]) && *argv[i] != '.')) {
            ntemps = i-1;
            break;
        }
        if (i == argc-1) {
            ntemps = i;
            break;
        }
    }
    if (ntemps < 2 || ntemps > 100) {
        fprintf(stderr, "Error: 2 to 100 temperatures required\n");
        return (1);
    }
    double *temps = new double[ntemps];
    for (int i = 0; i < ntemps; i++) {
        temps[i] = atof(argv[i+1]);
        if (temps[i] <= 0) {
            fprintf(stderr, "Error: non-positive temperature\n");
            return (1);
        }
    }

    char *datafile;
    char *dp;
    if (mmc_tcadir) {
        datafile = new char[strlen(mmc_tcadir) + 40];
        sprintf(datafile, "%s/", mmc_tcadir);
        dp = datafile + strlen(datafile);
    }
    else {
        datafile = new char[40];
        dp = datafile;
    }

    int err = 0;
    FILE *fp = 0;
    bool done_header = false;
    fp = fopen(datafile, "w");
    for (int i = 0; i < ntemps; i++) {
        int nt = ntemps;
        mm_create_data(argc-nt, argv+nt, temps[i], true);
        if (!done_header) {
            done_header = true;
            sprintf(dp, "tpt%03d%06ld%06ld%02ld%04d-%02d%03ld.pts", nt,
                lround(temps[0]*1e4), lround(temps[nt-1]*1e4),
                lround(mmc_sm*1e3), mmc_numxpts, mmc_nterms,
                lround(mmc_thr*1e3));
            fp = fopen(datafile, "w");
            if (!fp) {
                fprintf(stderr, "Error: failed to open %s for writing.\n",
                    datafile);
                err = 1;
                break;
            }
            fprintf(fp, "tpoints %d %.4f %.4f %.3f %d %d %3f\n", nt,
                temps[0], temps[nt-1], mmc_sm, mmc_numxpts, mmc_nterms,
                mmc_thr);
        }

        mm_create_fit(argc-nt, argv+nt, fp);
    }
    if (fp)
        fclose(fp);
    delete [] temps;
    delete [] datafile;
    return (err);
}


// Load the internal data registers from a TCA data file, as produced with
// the "cd" command.  This overwrites any existing TCA data.
//
// ld filename
int
mmjco_cmds::mm_load_data(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Error: no filename given.\n");
        return (1);
    }

    FILE *fp;
    char *path = 0;
    if (is_rooted(argv[1]) || !mmc_tcadir) {
        fp = fopen(argv[1], "r");
        if (!fp) {
            fprintf(stderr, "Error: unable to open file %s.\n", argv[1]);
            return (1);
        }
    }
    else {
        path = new char[strlen(mmc_tcadir) + strlen(argv[1]) + 2];
        sprintf(path, "%s/%s", mmc_tcadir, argv[1]);
        fp = fopen(path, "r");
        if (!fp) {
            delete [] path;
            fprintf(stderr, "Error: unable to open file %s.\n", argv[1]);
            return (1);
        }
    }
    char buf[256];

    // Read first line, determines file type.
    if (fgets(buf, 256, fp) == 0) {
        fprintf(stderr, "Error: file is empty or unreadable.\n");
        fclose(fp);
        return (1);
    }

    if (!strcmp(buf, "Title: mmjco")) {
        // Rawfile format for TCA data.

        double T, d1, d2, sm;
        int nxp = 0;
        while (fgets(buf, 256, fp) != 0) {
            if (sscanf(buf, "Plotname: %lf %lf %lf %lf %d", &T, &d1, &d2,
                    &sm, &nxp) == 5)
                break;
        }
        if (nxp != 0) {
            fprintf(stderr, "Error: bad or missing parameters.\n");
            fclose(fp);
            return (1);
        }

        bool iscplx = true;
        while (fgets(buf, 256, fp) != 0) {
            char d;
            if (sscanf(buf, "Flags: %c", &d) == 1)
                break;
            if (d == 'r' || d == 'R')
                iscplx = false;
        }

        int npts = 0;
        while (fgets(buf, 256, fp) != 0) {
            if (sscanf(buf, "No. Points: %d", &npts) == 1)
                break;
        }
        if (npts < 2) {
            fprintf(stderr, "Error: too few points.\n");
            fclose(fp);
            return (1);
        }
        if (npts != nxp) {
            fprintf(stderr, "Error: ambiguous point count.\n");
            fclose(fp);
            return (1);
        }

        bool fvals = false;
        while (fgets(buf, 256, fp) != 0) {
            if (!strcmp(buf, "Values:")) {
                fvals = true;
                break;
            }
        }
        if (!fvals) {
            fprintf(stderr, "Error: no valid data in file.\n");
            fclose(fp);
            return (1);
        }

        delete [] mmc_pair_data;
        delete [] mmc_qp_data;
        delete [] mmc_xpts;

        int ix = 0;
        int state = 0;
        if (iscplx) {
            while (fgets(buf, 256, fp) != 0) {
                if (state == 0) {
                    if (sscanf(buf, "%d %lf", &ix, mmc_xpts + ix) != 2)
                        break;
                    state++;
                    continue;
                }
                if (state == 1) {
                    double pr, pi;
                    if (sscanf(buf, "%lf %lf", &pr, &pi) != 2)
                        break;
                    mmc_pair_data[ix].real(pr);
                    mmc_pair_data[ix].imag(pi);
                    state++;
                    continue;
                }
                if (state == 1) {
                    double pr, pi;
                    if (sscanf(buf, "%lf %lf", &pr, &pi) != 2)
                        break;
                    mmc_qp_data[ix].real(pr);
                    mmc_qp_data[ix].imag(pi);
                    state = 0;
                    continue;
                }
            }
        }
        else {
            while (fgets(buf, 256, fp) != 0) {
                if (state == 0) {
                    if (sscanf(buf, "%d %lf", &ix, mmc_xpts + ix) != 2)
                        break;
                    state++;
                    continue;
                }
                if (state == 1) {
                    double d;
                    if (sscanf(buf, "%lf", &d) != 1)
                        break;
                    mmc_pair_data[ix].real(d);
                    state++;
                    continue;
                }
                if (state == 2) {
                    double d;
                    if (sscanf(buf, "%lf", &d) != 1)
                        break;
                    mmc_pair_data[ix].imag(d);
                    state++;
                    continue;
                }
                if (state == 3) {
                    double d;
                    if (sscanf(buf, "%lf", &d) != 1)
                        break;
                    mmc_qp_data[ix].real(d);
                    state++;
                    continue;
                }
                if (state == 4) {
                    double d;
                    if (sscanf(buf, "%lf", &d) != 1)
                        break;
                    mmc_qp_data[ix].imag(d);
                    state = 0;
                    continue;
                }
            }
        }
        if (ix != npts-1) {
            fprintf(stderr, "Error: problem reading data.\n");
            fclose(fp);
            return (1);
        }

        mmc_numxpts = npts;

        mmc_temp = T;
        mmc_d1 = d1;
        mmc_d2 = d2;
        mmc_sm = sm;
    }
    else {

        // Generic TCA format.
        int cnt = 0;
        while (fgets(buf, 256, fp) != 0) {
            int i;
            double x, pr, pi, qr, qi;
            if (sscanf(buf, "%d %lf %lf %lf %lf %lf",
                    &i, &x, &pr, &pi, &qr, &qi) == 6)
                cnt++;
        }
        if (cnt > 2) {
            fprintf(stderr, "Error: no valid data in file.\n");
            return (1);
        }

        delete [] mmc_pair_data;
        delete [] mmc_qp_data;
        delete [] mmc_xpts;

        rewind(fp);
        cnt = 0;
        while (fgets(buf, 256, fp) != 0) {
            int i;
            double x, pr, pi, qr, qi;
            if (sscanf(buf, "%d %lf %lf %lf %lf %lf",
                    &i, &x, &pr, &pi, &qr, &qi) == 6) {
                mmc_xpts[cnt] = x;
                mmc_pair_data[cnt].real(pr);
                mmc_pair_data[cnt].imag(pi);
                mmc_qp_data[cnt].real(qr);
                mmc_qp_data[cnt].imag(qi);
                cnt++;
            }
        }
        mmc_numxpts = cnt;
        // XXX maybe want to save these in the data file?
        mmc_temp = 0.0;
        mmc_d1 = 0.0;
        mmc_d2 = 0.0;
        mmc_sm = 0.0;
    }

    if (!path) {
        path = new char[strlen(argv[1]) + 1];
        strcpy(path, argv[1]);
    }
    delete [] mmc_datafile;
    mmc_datafile = path;
    return (0);
}


// Load the internal fitting parameter register from a file as
// produced with the "cf" command, or mitmojco.
//
// lf filename
int
mmjco_cmds::mm_load_fit(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Error: no filename given.\n");
        return (1);
    }

    FILE *fp;
    char *path = 0;
    if (is_rooted(argv[1]) || !mmc_tcadir) {
        fp = fopen(argv[1], "r");
        if (!fp) {
            fprintf(stderr, "Error: unable to open file %s.\n", argv[1]);
            return (1);
        }
    }
    else {
        path = new char[strlen(mmc_tcadir) + strlen(argv[1]) + 2];
        sprintf(path, "%s/%s", mmc_tcadir, argv[1]);
        fp = fopen(path, "r");
        if (!fp) {
            delete [] path;
            fprintf(stderr, "Error: unable to open file %s.\n", argv[1]);
            return (1);
        }
    }

    mmc_nterms = 0;
    mmc_thr = 0.0;
    delete [] mmc_fitfile;
    mmc_fitfile = path;
 
    // If it has a header line use it, otherwise just grab the numbers
    // for backwards compatibility.
    //
    double tp, d1, d2, sm, th;
    int xp, nt;
    bool ntset = false;
    if (fscanf(fp, "%*s %lf %lf %lf %lf %d %d %lf", &tp, &d1, &d2, &sm, &xp,
            &nt, &th) == 7) {
        mmc_temp = tp;
        mmc_d1 = d1;
        mmc_d2 = d2;
        mmc_sm = sm;
        mmc_numxpts = xp;
        mmc_nterms = nt;
        mmc_thr = th;
        ntset = true;
    }
    else
        rewind(fp);

    mmc_mf.load_fit_parameters(argv[1], fp);
    if (!ntset)
        mmc_nterms = mmc_mf.numterms();
    return (0);
}


// Load fit Parameter data interpolated from a sweep file.
//
// ls -fs filename temp
int
mmjco_cmds::mm_load_sweep(int argc, char **argv)
{
    mmjco_mtdb *mt;
    double temp;
    char *ffn;
    if (mm_get_sweep_fit(argc, argv, &mt, &temp, &ffn))
        return (1);

    // From now on, this is example-only code.  We load the new fit
    // parameters back into mmjco as if they were computed directly,
    // then drop a model file (back-converted TCA amplitudes) which
    // can be plotted or compared with an actual TCA amplitude file
    // for the temperature.

    // Print the parameters.
    const double *dp = mt->res_data();
    for (int i = 0; i < mt->num_terms(); i++) {
        fprintf(stdout, "%10.6f,%10.6f,%10.6f,%10.6f,%10.6f,%10.6f\n",
            dp[0], dp[1], dp[2], dp[3], dp[4], dp[5]);
        dp += 6;
    }

    mmc_mf.load_fit_parameters(mt->res_data(), mt->num_terms());

    // Clear TCA data if any, set parameters from the interpolation fit
    // data.
    delete [] mmc_pair_data;
    delete [] mmc_qp_data;
    mmc_pair_data = 0;
    mmc_qp_data = 0;
    mmc_d1 = 0.0;
    mmc_d2 = 0.0;
    mmc_temp = temp;

    mmc_nterms = mt->num_terms();
    mmc_thr = mt->thresh();
    if (mmc_numxpts != mt->num_xp()) {
        mmc_numxpts = mt->num_xp();
        delete [] mmc_xpts;
        mmc_xpts = new double[mmc_numxpts];

        double x0 = 0.001;
        double dx = (2.0 - x0)/(mmc_numxpts-1);
        for (int i = 0; i < mmc_numxpts; i++) {
            double xx = x0;
            // Need to avoid xx == 1.0 which causes integration failure.
            if (fabs(xx - 1.0) < 1e-5)
                xx = 1.0 + 1e-5;
            mmc_xpts[i] = xx;
            x0 += dx;
        }
    }
    mmc_sm = mt->smooth();
    delete [] mmc_datafile;
    mmc_datafile = 0;
    delete [] mmc_fitfile;
    mmc_fitfile = 0;

    char *fitfile = new char[strlen(ffn) + 16];
    strcpy(fitfile, ffn);
    delete [] ffn;
    char *t = strrchr(fitfile, '.');
    if (t && !strcmp(t, ".swp"))
        *t = 0;
    sprintf(fitfile + strlen(fitfile), "_%.4f.fit", temp);

    // Save a fit file.
    if (!mt->dump_file(fitfile)) {
        fprintf(stderr, "Error: write fit file failed.\n");
        delete mt;
        delete [] fitfile;
        return (1);
    }
    mmc_fitfile = fitfile;

    delete mt;
    return (0);
}


namespace {
    // Return the date. Return value is static data.
    //
    char *datestring()
    {
#define HAVE_GETTIMEOFDAY
#ifdef HAVE_GETTIMEOFDAY
        struct timeval tv;
        gettimeofday(&tv, 0);
        struct tm *tp = localtime((time_t *) &tv.tv_sec);
        char *ap = asctime(tp);
#else
        time_t tloc;
        time(&tloc);
        struct tm *tp = localtime(&tloc);
        char *ap = asctime(tp);
#endif

        static char tbuf[40];
        strcpy(tbuf, ap ? ap : "");
        int i = strlen(tbuf);
        tbuf[i - 1] = '\0';
        return (tbuf);
    }
}


// Save the TCA data in a file.
//
void
mmjco_cmds::save_data(const char *filename, FILE *fp, DFTYPE dtype,
    const double *x, const complex<double> *Jpair_data,
    const complex<double> *Jqp_data, int xsz)
{
    if (dtype == DFDATA) {
        // Save in generic format.
        fprintf(fp,
          "#    X            Jpair_real   Jpair_imag   Jqp_real     Jqp_imag\n");
        for (int i = 0; i < xsz; i++) {
            fprintf(fp, "%-4d %-12.5e %-12.5e %-12.5e %-12.5e %-12.5e\n",
                i, x[i],
                Jpair_data[i].real(), Jpair_data[i].imag(),
                Jqp_data[i].real(), Jqp_data[i].imag());
        }
        printf("TCA data saved to file %s.\n", filename);
    }
    else {
        // Save in rawfile format.
        char tbf[80];
        sprintf(tbf, "%11.4e %11.4e %11.4e %.3f %-4d", mmc_temp, mmc_d1,
            mmc_d2, mmc_sm, mmc_numxpts);

        fprintf(fp, "Title: mmjco\n");
        fprintf(fp, "Date: %s\n", datestring());
        fprintf(fp, "Plotname: %s\n", tbf);
        if (dtype == DFRAWCPLX) {
            fprintf(fp, "Flags: complex\n");
            fprintf(fp, "No. Variables: 3\n");
            fprintf(fp, "No. Points: %d\n", xsz);
            fprintf(fp, "Variables:\n 0 none X\n 1 Jpair\n 2 Jqp\n");
            fprintf(fp, "Values:\n");
            for (int i = 0; i < xsz; i++) {
                fprintf(fp,
                  " %d\t%12.5e,%12.5e\n\t%12.5e,%12.5e\n\t%12.5e,%12.5e\n",
                    i, x[i], 0.0, Jpair_data[i].real(), Jpair_data[i].imag(),
                    Jqp_data[i].real(), Jqp_data[i].imag());
            }
            printf("TCA data saved to complex rawfile %s.\n", filename);
        }
        else {
            fprintf(fp, "Flags: real\n");
            fprintf(fp, "No. Variables: 5\n");
            fprintf(fp, "No. Points: %d\n", xsz);
            fprintf(fp, "Variables:\n 0 none X\n 1 Jpair_re\n 2 "
                "Jpair_im\n 3 Jqp_re\n 4 Jqp_im\n");
            fprintf(fp, "Values:\n");
            for (int i = 0; i < xsz; i++) {
                fprintf(fp,
                  " %d\t%12.5e\n\t%12.5e\n\t%12.5e\n\t%12.5e\n\t%12.5e\n",
                    i, x[i], Jpair_data[i].real(), Jpair_data[i].imag(),
                    Jqp_data[i].real(), Jqp_data[i].imag());
            }
            printf("TCA data saved to real rawfile %s.\n", filename);
        }
    }
}


// Static function.
// Fill the av array with tokens from commandLine.  No allocation,
// spaces are replaced with null characters.
//
void
mmjco_cmds::get_av(char **av, int *ac, char *commandLine)
{
    *ac = 0;
    av[0] = 0;

    char *p2 = strtok(commandLine, " ");
    while (p2 && *ac < MAX_ARGC-1) {
        av[(*ac)++] = p2;
        p2 = strtok(0, " ");
    }
    av[*ac] = 0;
    int l = strlen(av[*ac-1]);
    av[*ac-1][l-1] = 0;
}
