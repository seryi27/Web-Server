#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <confuse.h>

int main(int argc, char *argv[])
{
    char *fichero;
    static char *server_root = NULL;
    static char *server_signature = NULL;
    static long int max_clients;
    static long int listen_port;
    /* Although the macro used to specify an integer option is called
     * CFG_SIMPLE_INT(), it actually expects a long int. On a 64 bit system
     * where ints are 32 bit and longs 64 bit (such as the x86-64 or amd64
     * architectures), you will get weird effects if you use an int here.
     *
     * If you use the regular (non-"simple") options, ie CFG_INT() and use
     * cfg_getint(), this is not a problem as the data types are implicitly
     * cast.
     */

    cfg_opt_t opts[] = {
        CFG_SIMPLE_STR("server_root", &server_root),
        CFG_SIMPLE_INT("max_clients", &max_clients),
        CFG_SIMPLE_INT("listen_port", &listen_port),
        CFG_SIMPLE_STR("server_signature", &server_signature),
        CFG_END()
    };

    fichero=strdup("server.conf");

    cfg_t *cfg;
    /* set default value for the server option */
    //server = strdup("gazonk");

    cfg = cfg_init(opts, 0);
    cfg_parse(cfg, fichero);


    printf("server_root: %s\n", server_root);
    printf("max_clients: %ld\n", max_clients);
    printf("listen_port: %ld\n", listen_port);
    printf("server_signature: %s\n", server_signature);

    //printf("setting username to 'foo'\n");
    /* using cfg_setstr here is not necessary at all, the equivalent
     * code is:
     *   free(username);
     *   username = strdup("foo");
     */
    //cfg_setstr(cfg, "user", "foo");
    //printf("username: %s\n", username);
    /* print the parsed values to another file */
    {
        FILE *fp = fopen("simple.conf.out", "w");
        cfg_print(cfg, fp);
        fclose(fp);
    }
    cfg_free(cfg);
    /* You are responsible for freeing string values. */
    free(server_root);
    free(server_signature);
    return 0;
}
