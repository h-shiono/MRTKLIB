/*------------------------------------------------------------------------------
* dumplex.c : dump lex ephemeris in lex log
*
* 2010/06/10 new
*-----------------------------------------------------------------------------*/
#include "mrtklib/rtklib.h"
#include "mrtklib/mrtk_context.h"

/* main ----------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    const char *usage="dumplex [-s sat] file";
    nav_t nav={0};
    lex_t lex={0};
    gtime_t tof; 
    char *file="";
    int i,sat=0,trl=0;
    mrtk_ctx_t *ctx = mrtk_ctx_create();

    for (i=0;i<argc;i++) {
        if      (!strcmp(argv[i],"-s")&&i+1<argc) sat=atoi(argv[++i]);
        else if (!strcmp(argv[i],"-x")&&i+1<argc) trl=atoi(argv[++i]);
        else if (!strcmp(argv[i],"-")) {
            fprintf(stderr,"usage: %s\n",usage);
            mrtk_ctx_destroy(ctx);
            return 0;
        }
        else file=argv[i];
    }
    if (trl>0) {
        traceopen(ctx,"dumplex.trace");
        tracelevel(ctx,trl);
    }
    if (!lexreadmsg(file,0,&lex)) {
        fprintf(stderr,"file read error: %s\n",file);
        mrtk_ctx_destroy(ctx);
        return -1;
    }
    for (i=0;i<lex.n;i++) {
        lexupdatecorr(lex.msgs+i,&nav,&tof);
    }
    if (trl>0) traceclose(ctx);
    mrtk_ctx_destroy(ctx);
    return 0;
}
