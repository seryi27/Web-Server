#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <confuse.h>

extern int, max_clients, listen_port;
extern char server_root[128];
extern char server_signature[128];


int ini_config(char *fichero)
{


    cfg_opt_t opts[] = {
      CFG_SIMPLE_STR("server_root", &server_root),
      CFG_SIMPLE_INT("max_clients", &max_clients),
      CFG_SIMPLE_INT("listen_port", &listen_port),
      CFG_SIMPLE_STR("server_signature", &server_signature),
      CFG_END()
    };

    cfg_t *cfg;

    cfg = cfg_init(opts, 0);
    cfg_parse(cfg, fichero);


    cfg_free(cfg);
    return 0;
}
