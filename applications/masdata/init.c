#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <libconfig.h>
#include <mcecmd.h>
#include <mceconfig.h>
#include <libmaslog.h>

#include "data.h"
#include "init.h"
#include "server.h"
#include "masdata.h"

#define SUBNAME "init: "
#define dprintf(A) //printf(SUBNAME A); fflush(stdout);

int init(params_t *p, int argc, char **argv)
{
	char errstr[MEDLEN];

	dprintf("entry\n");

	data_init(p);

	server_t *server = &p->server;

	if (check_usage(argc, argv)==0)
		return 1;

	dprintf("get config-file\n");

	char config_file[1024];
	if (get_config_file(config_file, argc, argv) != 0)
		strcpy(config_file, CONFIG_FILE);

	dprintf("load config?\n");

	if (load_config(p, config_file))
		return 2;

	dprintf("process options?\n");

	if (process_options(p, argc, argv)!=0)
		return 3;

	//Change working directory
	if (data_set_directory(p, errstr, NULL)) {
		fprintf(stderr, "%s\n", errstr);
	}

	//Set a frame size
	printf("Using first column option '%s'\n",
	       p->frame_setup.colcfgs[0].code);
	if (frame_set_card(&p->frame_setup, p->frame_setup.colcfgs[0].code,
			   errstr)) {
		fprintf(stderr, "Failure: %s\n", errstr);
		return 4;
	}
	

	printf("Frame size = %i, data size = %i\n",
	       p->frame_setup.frame_size,
	       p->frame_setup.data_size);
	fflush(stdout);

	if (listener_init(&server->listener, MAX_CLIENTS,
			  MAX_MSG, MAX_MSG) != 0)
		return 4;

	//Lookup parameter ids
	if (mceids_config(p) != 0)
		return 5;

/* 	//Check.. */
/* 	printf("%x %x %x %x\n", */
/* 	       p->mce_comms.seq_card_id, */
/* 	       p->mce_comms.seq_cmd_id, */
/* 	       p->frame_setup.colcfgs[0].card_id, */
/* 	       p->frame_setup.colcfgs[0].para_id); */

        //Connect to MCE driver
	mce_comms_t *mce_comms = &p->mce_comms;
	mce_comms->mcecmd_handle = mce_open(mce_comms->mcecmd_name);
	if (mce_comms->mcecmd_handle < 0) {
		fprintf(stderr, "Could not connect to mce command device '%s'\n",
			mce_comms->mcecmd_name);
		return 6;
	}

	//Open data stream
	mce_comms->mcedata_fd = open(mce_comms->mcedata_name, O_RDONLY | O_NONBLOCK);
	if (mce_comms->mcedata_fd <= 0) {
		fprintf(stderr, "Could not connect to mce data device '%s'\n",
			mce_comms->mcedata_name);
		return 7;
	}
	if (fcntl(mce_comms->mcedata_fd, F_SETFL,
		  fcntl(0,F_GETFL) | O_NONBLOCK) == -1) {
		fprintf(stderr, "Failed to make mce data device non-blocking.\n");
		return 7;
	}

	//Start logging
	if ( logger_connect(&p->logger, NULL, "data_task") != 0) {
		fprintf(stderr, "logger_connect failed, no logging.\n");
	}

	//Start listening
	if (listener_listen(&server->listener, server->serve_address)!=0)
		return 8;

	return 0;
}

#undef SUBNAME

#define SUBNAME "load_config: "


config_setting_t *get_setting(config_setting_t *parent,
			      const char *name)
{
	config_setting_t *set =
		config_setting_get_member(parent, name);
	if (set==NULL) {
		fprintf(stderr, SUBNAME
			"key '%s' not found in config file\n",
			name);
		return NULL;
	}
	return set;
}

int get_integer(int *dest,
		config_setting_t *parent, const char *name)
{
	config_setting_t *set = get_setting(parent,name);
	if (set==NULL) return -1;
	*dest = config_setting_get_int(set);
	return 0;
}

int get_string(char *dest,
	       config_setting_t *parent, const char *name)
{
	config_setting_t *set = get_setting(parent,name);
	if (set==NULL) return -1;
	strcpy(dest, config_setting_get_string(set));
	return 0;
}


int get_string_to_key(int *dest,
		      config_setting_t *parent, const char *name,
		      struct string_pair *pairs, int count)
{
	char setting[SHORTLEN];
	if (get_string(setting, parent, name)!=0) return -1;
	
	int key;
	for (key=0; key<count; key++)
		if (strcmp(pairs[key].name, setting)==0) {
			*dest = key;
			return 0;
		}

	fprintf(stderr, SUBNAME
		"Invalid argument '%s' to setting '%s'\n",
		setting, name);

	return 0;
}

int destroy_exit(struct config_t *cfg, int error) {
	config_destroy(cfg);
	return error;
}

int load_config(params_t *p, char *config_file)
{
	char errstr[MEDLEN];

	struct config_t cfg;
	config_init(&cfg);

	if (config_file!=NULL) {
		if (!config_read_file(&cfg, config_file)) {
			fprintf(stderr,
				SUBNAME "Could not read config file '%s'\n",
				config_file);
			return -1;
		}
	} else {
		if (!config_read_file(&cfg, CONFIG_FILE)) {
			fprintf(stderr, SUBNAME
				"Could not read default configfile '%s'\n",
				CONFIG_FILE);
			return -1;
		}
	}


	dprintf(" get server options...\n");

	server_t *server = &p->server;
	config_setting_t *server_config = config_lookup(&cfg, CONFIG_SERVER);
	if (server_config==NULL) {
		fprintf(stderr, SUBNAME "could not find '%s' section "
			"in config file\n", CONFIG_SERVER);
		return destroy_exit(&cfg, -1);
	}

	//Now load the server settings
	if (get_string(server->serve_address, server_config, CONFIG_ADDR)!=0)
		return destroy_exit(&cfg, -1);


	dprintf(" get mce code options...\n");
	mce_comms_t *mce = &p->mce_comms;

 	if (get_string(mce->mcecmd_name, server_config, CONFIG_MCECMD)!=0)
 		return destroy_exit(&cfg, -1);

 	if (get_string(mce->mcedata_name, server_config, CONFIG_MCEDATA)!=0)
 		return destroy_exit(&cfg, -1);

 	if (get_string(mce->hardware_file, server_config, CONFIG_MCECFG)!=0)
 		return destroy_exit(&cfg, -1);

 	if (get_string(mce->seq_card_str, server_config, CONFIG_MCESEQCARD)!=0)
 		return destroy_exit(&cfg, -1);

 	if (get_string(mce->seq_cmd_str, server_config, CONFIG_MCESEQCMD)!=0)
 		return destroy_exit(&cfg, -1);

 	if (get_string(mce->go_cmd_str, server_config, CONFIG_MCEGOCMD)!=0)
 		return destroy_exit(&cfg, -1);

        //Non-fatal
	get_integer(&server->daemon, server_config, CONFIG_DAEMON);
 	if (get_string(server->work_dir, server_config, CONFIG_DIR)!=0)
 		server->work_dir[0] = 0;

        //Frame structure

	dprintf(" get frame options...\n");

	frame_setup_t *frame = &p->frame_setup;

	char form_str[SHORTLEN] = "";
	get_string(form_str, server_config, CONFIG_FORMAT);
	if ( frame_set_format(frame, form_str, errstr) != 0) {
		fprintf(stderr, SUBNAME "%s=%s: %s\n",
			CONFIG_FORMAT, form_str, errstr);
 		return destroy_exit(&cfg, -1);
	}

	get_integer(&frame->header,  server_config, CONFIG_HEADER);
	get_integer(&frame->rows,    server_config, CONFIG_ROWS);
	get_integer(&frame->columns, server_config, CONFIG_COLUMNS);
	get_integer(&frame->extra,   server_config, CONFIG_EXTRA);
	get_integer(&frame->columns_card, server_config, CONFIG_COLUMNSCARD);

	config_setting_t *colopt =
		config_setting_get_member(server_config, CONFIG_COLOPT);
	if (colopt!=NULL) {

		config_setting_t *col;
		int cidx = 0;
		while ( (col=config_setting_get_elem(colopt, cidx)) !=NULL ) {
			if (cidx > MAX_COLCONF) {
				fprintf(stderr, SUBNAME
					"MAX_COLCONF=%i exceeded, some column "
					"options not available.\n",
					MAX_COLCONF);
				break;
			}
			get_string(frame->colcfgs[cidx].code, col,
				   CONFIG_COLSEL);
			get_integer(&frame->colcfgs[cidx].cards, col,
				    CONFIG_COLCRD);
			get_integer(&frame->colcfgs[cidx].card_idx, col,
				    CONFIG_COLCRDI);
			cidx++;
			
		}
		frame->colcfg_count = cidx;
		frame->colcfg_index = 0;
	}
	
	// Data file configuration

	datafile_t *datafile = &p->datafile;
	config_setting_t *datafile_cfg =
		config_setting_get_member(server_config, CONFIG_DATAFILE);
	if (datafile_cfg != NULL) {
		char s[MEDLEN];
		char errstr[MEDLEN] = "";
		int i;
		if (get_string(s, datafile_cfg, CONFIG_FILEMODE)==0) {
			if (data_set_filemode(datafile, s, errstr)!=0) {
				fprintf(stderr, SUBNAME "%s=%s: %s\n",
					CONFIG_FILEMODE, s, errstr);
				return destroy_exit(&cfg, -1);
			}
		}
		if (get_string(s, datafile_cfg, CONFIG_BASENAME)==0) {
			if (data_set_basename(datafile, s, errstr)!=0) {
				fprintf(stderr, SUBNAME "%s=%s: %s\n",
					CONFIG_BASENAME, s, errstr);
				return destroy_exit(&cfg, -1);
			}
		}
		if (get_integer(&i, datafile_cfg, CONFIG_FRAMEINT)==0) {
			if (data_set_frameint(datafile, i, errstr)!=0) {
				fprintf(stderr, SUBNAME "%s=%s: %s\n",
					CONFIG_FRAMEINT, s, errstr);
				return destroy_exit(&cfg, -1);
			}
		}
	}

	return destroy_exit(&cfg, 0);
}

#undef SUBNAME

#define SUBNAME "mceids_config: "

int mceids_config(params_t *p) {

	if (p==NULL) return -1;
	mce_comms_t *mce = &p->mce_comms;
	
	mce_data_t *mcedata;

	if (mceconfig_load(mce->hardware_file, &mcedata)!=0) {
		fprintf(stderr, SUBNAME
			"error loading hardware configuration file\n");
		return -1;
	}

	param_properties_t props;

	//t count, type = MCE_PARAM_INT;
	if (mceconfig_lookup(&props, mcedata,
			     mce->seq_card_str, mce->seq_cmd_str, 0)!=0) {
		fprintf(stderr, SUBNAME "failed to lookup pair '%s:%s'\n",
			mce->seq_card_str, mce->seq_cmd_str);
		return -1;
	}
	mce->seq_card_id = props.card_id;
 	mce->seq_cmd_id = props.para_id;

	int i;
	for (i=0; i < p->frame_setup.colcfg_count; i++) {
		column_config_t *c = &p->frame_setup.colcfgs[i];
		
		if (mceconfig_lookup(&props, mcedata,
				     c->code, mce->go_cmd_str, 0) != 0) {
			fprintf(stderr, SUBNAME "failed to lookup pair "
				"'%s:%s'\n", c->code, mce->go_cmd_str);
			return -1;
		}

		c->card_id = props.card_id;
		c->para_id = props.para_id;
	}

	free(mcedata);

	return 0;
}

#undef SUBNAME


#define OPT "f:o:F:D:h"
#define USAGE_OPT \
"  -f cfgfile        use alternate configuration file\n" \
"  -o datafile       select default data file\n" \
"  -D [0 | 1]        enable/disable daemon mode\n"

int check_usage(int argc, char **argv) {
	optind = 0;
	int option;
	while ( (option = getopt(argc, argv, OPT)) >=0) {

		switch(option) {
		case '?':
		case 'h':
			printf("Usage:\n\t%s [options]\n"
			       " Options\n" USAGE_OPT,
			       argv[0]);
			return 0;
		}
	}
	return -1;
}

int get_config_file(char *filename, int argc, char **argv)
{
	optind = 0;
	int option;
	while ( (option = getopt(argc, argv, OPT )) >=0) {
		if (option=='f') {
			strcpy(filename, optarg);
			return 0;
		}
	}
	return -1;	
}

int process_options(params_t *p, int argc, char **argv)
{
	server_t *server = &p->server;

	optind = 0;
 	int option;
	while ( (option = getopt(argc, argv, OPT)) >=0) {

		switch(option) {
		case '?':

		case 'h':
			printf("Usage:\n\t%s [options]\n"
			       " Options\n" USAGE_OPT,
			       argv[0]);
			return -1;
			
		case 'f':
			//Preloaded, ignore.
			break;

/*		case 'o':
			strcpy(server->filename, optarg);
			break;

		case 'F':
			if (optarg[0]=='0') data_clearflag(p, FLAG_FLUSH);
			else if (optarg[0]=='1') data_setflag(p, FLAG_FLUSH);
			else fprintf(stderr,
				     "Invalid -F argument, ignoring.\n");
			break;
*/
		case 'D':
			if (optarg[0]=='0') server->daemon = 0;
			else if (optarg[0]=='1') server->daemon = 1;
			else fprintf(stderr,
				     "Invalid -D argument, ignoring.\n");
			break;
	
		default:
			printf("Unimplemented option '-%c'!\n", option);
		}
	}
	return 0;
}
