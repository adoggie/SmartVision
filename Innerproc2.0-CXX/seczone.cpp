#include "seczone.h"

#include <fstream>
#include <ostream>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

#ifdef  BOOST_OS_LINUX
#include <sys/ioctl.h>
#endif


#define DEV_GPIO "/dev/sunxi_gpio"
#define GPIO_IOC_MAGIC   'G'
#define IOCTL_GPIO_SETOUTPUT  _IOW(GPIO_IOC_MAGIC, 0, int)
#define IOCTL_GPIO_SETINPUT  _IOW(GPIO_IOC_MAGIC, 1, int)
#define IOCTL_GPIO_SETVALUE  _IOW(GPIO_IOC_MAGIC, 2, int)
#define IOCTL_GPIO_GETVALUE  _IOR(GPIO_IOC_MAGIC, 3, int)
#define GPIO_TO_PIN(bank, gpio) (32 * (bank-'A') + (gpio))

typedef struct {
	int pin;
	int data;
}am335x_gpio_arg;

seczone_conf_t g_seczone_conf[ZONE_NUMBER]; //max 16
int g_seczone_count = 0;    //实际的防区数量
//app_client_t *g_app_client_list = NULL;
char g_seczone_passwd[512];

int g_seczone_change = 0; //conf is change
//pthread_mutex_t seczone_mutex;\
int g_onekey_set = 0;

int g_seczone_mode = 0;


//char g_seczone_mode_conf[MODE_NUMBER][64];

		/*
seczone_mode_info_t g_seczone_mode_conf[MODE_NUMBER];


void get_gpio_ncno() {
	int i = 0;
	for (i = 0; i < g_seczone_count; i++) {
		if (strcmp(g_seczone_conf[i].normalstate, "no") == 0) {
			g_seczone_conf[i].gpiolevel = 0;
		}
		if (strcmp(g_seczone_conf[i].normalstate, "nc") == 0) {
			g_seczone_conf[i].gpiolevel = 1;
		}
	}
	
	
}

void parse_secmode_to_seczone(int i) {
    {
		sscanf(g_seczone_mode_conf[i], "%[a-z]$%[a-z]$%[a-z]$%[a-z]$%[a-z]$%[a-z]$%[a-z]$%[a-z]",
			   g_seczone_conf[0].onekeyset,
			   g_seczone_conf[1].onekeyset,
			   g_seczone_conf[2].onekeyset,
			   g_seczone_conf[3].onekeyset,
			   g_seczone_conf[4].onekeyset,
			   g_seczone_conf[5].onekeyset,
			   g_seczone_conf[6].onekeyset,
			   g_seczone_conf[7].onekeyset);
	}
}
		 
		 */
		/*
int get_seczone_conf() {
	dictionary *ini;
	
	int b = 0;
	int i = 0;
	double d;
	char *s;
	char zonename[64];
	memset(zonename, 0, 64);
	
	char modename[64];
	memset(modename, 0, 64);
	
	ini = iniparser_load(SECZONE_CONF);
	if (ini == NULL) {
		fprintf(stderr, "cannot parse file: %s\n", SECZONE_CONF);
		return -1;
	}
	
	s = iniparser_getstring(ini, "password:pass", NULL);
	if (s)
		strncpy(g_seczone_passwd, s, 512);
	
	
	g_seczone_count = iniparser_getint(ini, "zonenumber:count", 0);
	DEBUG_INFO("#####g_seczone_count:%d", g_seczone_count);
	
	g_onekey_set = iniparser_getint(ini, "onekeyset:state", 0);
	DEBUG_INFO("#####g_onekey_set:%d", g_onekey_set);
	
	g_seczone_mode = iniparser_getint(ini, "seczonemode:mode", 0);
	
	for (i = 0; i < MODE_NUMBER; i++) {
		
		sprintf(modename, "mode%d:value", i);
		s = iniparser_getstring(ini, modename, NULL);
		if (s)
			strncpy(g_seczone_mode_conf[i], s, 64);
	}
	
	g_onekey_set_func();
	
	for (i = 0; i < g_seczone_count; i++) {
		sprintf(zonename, "zone%d:port", i + 1);
		b = iniparser_getint(ini, zonename, 0);
		g_seczone_conf[i].port = b;
		
		sprintf(zonename, "zone%d:name", i + 1);
		s = iniparser_getstring(ini, zonename, NULL);
		if (s)
			strncpy(g_seczone_conf[i].name, s, 128);
		
		sprintf(zonename, "zone%d:normalstate", i + 1);
		s = iniparser_getstring(ini, zonename, NULL);
		if (s)
			strncpy(g_seczone_conf[i].normalstate, s, 16);
		
		sprintf(zonename, "zone%d:onekeyset", i + 1);
		s = iniparser_getstring(ini, zonename, NULL);
		if (s)
			strncpy(g_seczone_conf[i].onekeyset, s, 8);
		
		sprintf(zonename, "zone%d:currentstate", i + 1);
		s = iniparser_getstring(ini, zonename, NULL);
		if (s)
			strncpy(g_seczone_conf[i].currentstate, s, 8);
		
		sprintf(zonename, "zone%d:delaytime", i + 1);
		b = iniparser_getint(ini, zonename, 0);
		g_seczone_conf[i].delaytime = b;
		
		sprintf(zonename, "zone%d:nursetime", i + 1);
		b = iniparser_getint(ini, zonename, 0);
		g_seczone_conf[i].nursetime = b;
		
		sprintf(zonename, "zone%d:alltime", i + 1);
		s = iniparser_getstring(ini, zonename, NULL);
		if (s)
			strncpy(g_seczone_conf[i].alltime, s, 8);
		
		sprintf(zonename, "zone%d:gpiolevel", i + 1);
		if (s)
			g_seczone_conf[i].gpiolevel = iniparser_getint(ini, zonename, 0);
		
		sprintf(zonename, "zone%d:triggertype", i + 1);
		g_seczone_conf[i].triggertype = iniparser_getint(ini, zonename, 0);
		
		sprintf(zonename, "zone%d:online", i + 1);
		s = iniparser_getstring(ini, zonename, NULL);
		if (s)
			strncpy(g_seczone_conf[i].online, s, 8);
	}
	
	iniparser_freedict(ini);
	get_gpio_ncno();
	
	return 0;
}

		 
		 */
		
/*
void write_seczone_conf_file(void) {
	g_onekey_set_func();
	
	FILE *ini;
	int i = 0;
	ini = fopen(SECZONE_CONF, "w");
	fprintf(ini,
			"#\n"
			"# secure zone configure file\n"
			"#\n"
			"\n"
			"[password]\n"
			"pass = %s ;\n"
			"\n"
			"\n"
			"[onekeyset]\n"
			"state = %d ;\n"
			"\n"
			"\n"
			"[seczonemode]\n"
			"mode = %d ;\n"
			"\n"
			"\n",
			g_seczone_passwd,
			g_onekey_set,
			g_seczone_mode);
	
	for (i = 0; i < MODE_NUMBER; i++) {
		fprintf(ini,
				"[mode%d]\n"
				"value = %s ;\n"
				"\n"
				"\n",
				i,
				g_seczone_mode_conf[i]);
	}
	
	fprintf(ini, "[zonenumber]\n"
				 "count = %d ;\n\n\n", g_seczone_count);
	
	for (i = 0; i < g_seczone_count; i++) {
		fprintf(ini,
				"[zone%d]\n"
				"port = %d ;\n"
				"name = %s ;\n"
				"normalstate = %s ;\n"
				"onekeyset = %s ;\n"
				"currentstate = %s ;\n"
				"delaytime = %d ;\n"
				"nursetime = %d ;\n"
				"alltime = %s ;\n"
				"gpiolevel = %d ;\n"
				"triggertype = %d ;\n"
				"online = %s ;\n"
				"\n"
				"\n",
				i + 1,
				g_seczone_conf[i].port,
				g_seczone_conf[i].name,
				g_seczone_conf[i].normalstate,
				g_seczone_conf[i].onekeyset,
				g_seczone_conf[i].currentstate,
				g_seczone_conf[i].delaytime,
				g_seczone_conf[i].nursetime,
				g_seczone_conf[i].alltime,
				g_seczone_conf[i].gpiolevel,
				g_seczone_conf[i].triggertype,
				g_seczone_conf[i].online
		);
	}
	fflush(ini);
	fclose(ini);
}
*/
/*
cJSON *seczone_pass_set(jrpc_context *ctx, cJSON *params, cJSON *id) {
	//do sec zone password set
	//compare the old pass
	cJSON *oldpass, *newpass;
	app_client_t *procer;
	char event[512];
	char timestr[64];
	memset(event, 0, 512);
	memset(timestr, 0, 64);
	
	if (access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	
	if (params != NULL && params->type == cJSON_Object) {
		oldpass = cJSON_GetObjectItem(params, "oldpass");
		if (oldpass != NULL && oldpass->type == cJSON_String) {
			if (strcmp(g_seczone_passwd, oldpass->valuestring) == 0 || strlen(g_seczone_passwd) == 0) {
				newpass = cJSON_GetObjectItem(params, "newpass");
				if (newpass != NULL && newpass->type == cJSON_String)
					strncpy(g_seczone_passwd, newpass->valuestring, 512);
				else
					return cJSON_CreateString("seczone passwd must not be null!");
			} else
				return cJSON_CreateString("wrong old seczone passwd!");
			
		} else
			return cJSON_CreateString("wrong old seczone passwd!");
		
	}
	
	//log it
	get_time_now(timestr);
	procer = get_appt_by_ctx(ctx);
	//snprintf(event,512,"防区密码设置(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
	snprintf(event, 512, "防区密码设置");
	seczonehistory_log_to_db(timestr, "设置", "防区配置", event);
	
	pthread_mutex_lock(&seczone_mutex);
	g_seczone_change = 1;
	pthread_mutex_unlock(&seczone_mutex);
	//write_seczone_conf_file();
	
	return cJSON_CreateString("seczone_pass_set_has_done");
}
 


void init_seczone_conf_t() {
	int i = 0;
	
	for (i = 0; i < ZONE_NUMBER; i++) {
		g_seczone_conf[i].port = i + 1;
		memset(g_seczone_conf[i].name, 0, 128);
		memset(g_seczone_conf[i].normalstate, 0, 16);
		memset(g_seczone_conf[i].onekeyset, 0, 8);
		memset(g_seczone_conf[i].currentstate, 0, 8);
		g_seczone_conf[i].delaytime = 0;
		g_seczone_conf[i].delaycount = 0;
		memset(g_seczone_conf[i].alltime, 0, 8);
		memset(g_seczone_conf[i].online, 0, 8);
		g_seczone_conf[i].gpiolevel = 0;
		g_seczone_conf[i].triggertype = 0;
		g_seczone_conf[i].etime = 0;
		g_seczone_conf[i].nursetime = 0;
		g_seczone_conf[i].lastfailtime = 0;
	}
}

void travel_seczone_conf() {
	int i = 0;
	DEBUG_INFO("seczone pass:%s", g_seczone_passwd);
	for (i = 0; i < g_seczone_count; i++) {
		DEBUG_INFO("seczone:%d %s %s", g_seczone_conf[i].port, g_seczone_conf[i].name, g_seczone_conf[i].normalstate);
	}
	
}
 */


/*
cJSON *destroy_emergency(jrpc_context *ctx, cJSON *params, cJSON *id) {
	cJSON *password;
	GET_LOCAL_ADDRESS();
	if (params->type == cJSON_Object) {
		password = cJSON_GetObjectItem(params, "password");
		if (password != NULL && password->type == cJSON_String) {
			if (strcmp(password->valuestring, g_seczone_passwd) != 0)
				return cJSON_CreateString("destroy_emergency_wrong_password");
			else {
				char timestr[128];
				bzero(timestr, 128);
				get_time_now(timestr);
				char msgtoproperty[1024];
				bzero(msgtoproperty, 1024);
				// scott 2018.12.3
//				snprintf(msgtoproperty,1024,"{\"method\":\"destroy_emergency\",\"params\":{\"time\":\%s\",\"ipaddr\":\"%s\",\"seczone\":\"\"}}",timestr,g_baseconf.outerinterfaceip);
				snprintf(msgtoproperty, 1024,
						 "{\"method\":\"destroy_emergency\",\"params\":{\"time\":\"%s\",\"ipaddr\":\"%s\",\"seczone\":\"\"}}",
						 timestr, g_baseconf.innerinterfaceip);
				send_msg(g_baseconf.alarmcenterip, msgtoproperty, PROPERTY_PORT);
				
				return cJSON_CreateString("destroy_emergency_success");
			}
		} else return cJSON_CreateString("destroy_emergency_null_parameters");
	} else return cJSON_CreateString("destroy_emergency_wrong_parameters");
	
}
 
 */


/*
cJSON *verify_emergency_password(jrpc_context *ctx, cJSON *params, cJSON *id) {
	cJSON *password;
	if (params->type == cJSON_Object) {
		password = cJSON_GetObjectItem(params, "password");
		if (password != NULL && password->type == cJSON_String) {
			if (strcmp(password->valuestring, g_seczone_passwd) != 0)
				return cJSON_CreateString("verify_emergency_password_false");
			else {
				
				return cJSON_CreateString("verify_emergency_password_true");
			}
		} else return cJSON_CreateString("null_params");
	} else return cJSON_CreateString("wrong_parameters");
	
}
 
 */


/*
cJSON *seczone_conf_set(jrpc_context *ctx, cJSON *params, cJSON *id) {
	//do sec zone config set
	cJSON *password, *config, *item;
	cJSON *port, *name, *normalstate, *onekeyset, *currentstate, *delaytime, *nursetime, *alltime, *online;
	cJSON *triggertype, *gpiolevel;
	int arraysize = 0, i = 0;
	app_client_t *procer;
	char event[512];
	char timestr[64];
	memset(event, 0, 512);
	memset(timestr, 0, 64);
	
	if (access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	
	char *jsonprintstr = cJSON_Print(params);
	DEBUG_INFO("seczone_conf_set params:%s", jsonprintstr);
	free(jsonprintstr);
	
	if (params->type == cJSON_Object) {
		password = cJSON_GetObjectItem(params, "password");
		if (password != NULL && password->type == cJSON_String) {
			//do conf get
//			if(1) {
			DEBUG_INFO("g_seczone_passwd:%s", g_seczone_passwd);
			if (strcmp(password->valuestring, g_seczone_passwd) == 0) {
//			if(1) {
				config = cJSON_GetObjectItem(params, "config");
				if (config != NULL && config->type == cJSON_Array) {
					arraysize = cJSON_GetArraySize(config);
					while (i < arraysize) {
						
						item = cJSON_GetArrayItem(config, i);
						if (item != NULL && item->type == cJSON_Object) {
							int k = 0;
							port = cJSON_GetObjectItem(item, "port");
							if (port != NULL && port->type == cJSON_Number) {
								if (port->valueint <= ZONE_NUMBER && port->valueint > 0)
									k = port->valueint - 1;
							}
							
							g_seczone_conf[k].port = k + 1;
							
							name = cJSON_GetObjectItem(item, "name");
							if (name != NULL && name->type == cJSON_String)
								strncpy(g_seczone_conf[k].name, name->valuestring, 128);
							DEBUG_INFO("port->valueint:%d %d", port->valueint, i);
							normalstate = cJSON_GetObjectItem(item, "normalstate");
							if (normalstate != NULL && normalstate->type == cJSON_String)
								strncpy(g_seczone_conf[k].normalstate, normalstate->valuestring, 16);
							
							onekeyset = cJSON_GetObjectItem(item, "onekeyset");
							if (onekeyset != NULL && onekeyset->type == cJSON_String)
								strncpy(g_seczone_conf[k].onekeyset, onekeyset->valuestring, 8);
							
							currentstate = cJSON_GetObjectItem(item, "currentstate");
							if (currentstate != NULL && currentstate->type == cJSON_String)
								strncpy(g_seczone_conf[k].currentstate, currentstate->valuestring, 8);
							
							delaytime = cJSON_GetObjectItem(item, "delaytime");
							if (delaytime != NULL && delaytime->type == cJSON_Number)
								g_seczone_conf[k].delaytime = delaytime->valueint;
							
							alltime = cJSON_GetObjectItem(item, "alltime");
							if (alltime != NULL && alltime->type == cJSON_String)
								strncpy(g_seczone_conf[k].alltime, alltime->valuestring, 8);
							
							gpiolevel = cJSON_GetObjectItem(item, "gpiolevel");
							if (gpiolevel != NULL && gpiolevel->type == cJSON_Number)
								g_seczone_conf[k].gpiolevel = gpiolevel->valueint;
							
							triggertype = cJSON_GetObjectItem(item, "triggertype");
							if (triggertype != NULL && triggertype->type == cJSON_Number)
								g_seczone_conf[k].triggertype = triggertype->valueint;
							
							online = cJSON_GetObjectItem(item, "online");
							if (online != NULL && online->type == cJSON_String)
								strncpy(g_seczone_conf[k].online, online->valuestring, 8);
							
							nursetime = cJSON_GetObjectItem(item, "nursetime");
							if (nursetime != NULL && nursetime->type == cJSON_Number)
								g_seczone_conf[k].nursetime = nursetime->valueint;
						}
						i++;
						
					}
				}
			} else
				return cJSON_CreateString("wrong_seczone_password");
		} else
			return cJSON_CreateString("null_seczone_password");
	} else
		return cJSON_CreateString(WRONG_PARAMETER);
	
	get_gpio_ncno();
	
	if (params) {
		travel_seczone_conf();
		pthread_mutex_lock(&seczone_mutex);
		g_seczone_change = 1;
		pthread_mutex_unlock(&seczone_mutex);
		//write_seczone_conf_file();
	}
	
	//log it
	get_time_now(timestr);
	procer = get_appt_by_ctx(ctx);
	//snprintf(event,512,"防区参数设置(by %s %s %s %s)",procer->name,procer->app_dev_id,procer->ip_addr_str,procer->type);
	snprintf(event, 512, "防区参数设置");
	seczonehistory_log_to_db(timestr, "设置", "防区配置", event);
	
	return cJSON_CreateString("seczone_conf_set_has_done");
}

//packet 16 create
cJSON *seczone_conf_p16_create() {
	int i = 0;
	cJSON *result_root = cJSON_CreateObject();
	cJSON *content_array = cJSON_CreateArray();
	cJSON *content_obj = NULL;
	
	for (i = 0; i < g_seczone_count; i++) {
		
		content_obj = cJSON_CreateObject();
		cJSON_AddNumberToObject(content_obj, "port", g_seczone_conf[i].port);
		cJSON_AddStringToObject(content_obj, "name", g_seczone_conf[i].name);
		cJSON_AddStringToObject(content_obj, "normalstate", g_seczone_conf[i].normalstate);
		cJSON_AddNumberToObject(content_obj, "triggertype", g_seczone_conf[i].triggertype);
		cJSON_AddNumberToObject(content_obj, "gpiolevel", g_seczone_conf[i].gpiolevel);
		cJSON_AddStringToObject(content_obj, "onekeyset", g_seczone_conf[i].onekeyset);
		cJSON_AddStringToObject(content_obj, "currentstate", g_seczone_conf[i].currentstate);
		cJSON_AddNumberToObject(content_obj, "delaytime", g_seczone_conf[i].delaytime);
		cJSON_AddNumberToObject(content_obj, "nursetime", g_seczone_conf[i].nursetime);
		cJSON_AddStringToObject(content_obj, "online", g_seczone_conf[i].online);
		cJSON_AddStringToObject(content_obj, "alltime", g_seczone_conf[i].alltime);
		
		cJSON_AddItemToArray(content_array, content_obj);
		
		DEBUG_INFO("seczone: %d %d %d %s %s", ZONE_NUMBER, i, g_seczone_conf[i].port, g_seczone_conf[i].name,
				   g_seczone_conf[i].normalstate);
		
	}
	
	cJSON_AddItemToObject(result_root, "seczone_conf", content_array);
	
	return result_root;
}
*/


/*
cJSON *seczone_conf_require(jrpc_context *ctx, cJSON *params, cJSON *id) {
	
	if (access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	//get seczone mode from params
	
	return seczone_conf_p16_create();
}


void *seczone_fileproc_thread() {
	int i = 0;
	int j = 0;
	while (1) {
		
		if (g_seczone_change == 1) {
			write_seczone_conf_file();
			j = 1;
			pthread_mutex_lock(&seczone_mutex);
			g_seczone_change = 0;
			pthread_mutex_unlock(&seczone_mutex);
		} else {
			//backup seczone conf per 60s
			//when file is changed
			i++;
			if ((i = 6) && (j == 1)) {
				backup_seczone_conf_file();
				i = 0;
				j = 0;
			}
		}
		
		thread_sleep(10);
	}
}

void seczonehistory_log_to_db(char *time, char *type, char *seczone, char *event) {
	char sqlstr[1024];
	memset(sqlstr, 0, 1024);
	
	snprintf(sqlstr, 1024, "insert into seczonehistory values ('%s', '%s', '%s', '%s')", time, type, seczone, event);
#ifdef SQL
	insert_data_to_db(sqlstr);
#endif
}
*/

/*
cJSON *seczone_mode_set(jrpc_context *ctx, cJSON *params, cJSON *id) {
	if (access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	
	cJSON *mode = NULL;
	
	if (params != NULL && params->type == cJSON_Object) {
		mode = cJSON_GetObjectItem(params, "mode");
		if (mode != NULL && mode->type == cJSON_Number)
			g_seczone_mode = mode->valueint;
		
	}
	
	pthread_mutex_lock(&seczone_mutex);
	g_seczone_change = 1;
	pthread_mutex_unlock(&seczone_mutex);
	
	//write_seczone_conf_file();
	parse_secmode_to_seczone(g_seczone_mode);
	
	return NULL;
}

cJSON *seczone_mode_get(jrpc_context *ctx, cJSON *params, cJSON *id) {
	if (access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	
	char mode[256];
	bzero(mode, 256);
	snprintf(mode, 256, "{\"result\":\"seczone_mode\",\"mode\":%d}^", g_seczone_mode);
	app_client_t *procer = get_appt_by_ctx(ctx);
	if (procer) {
		write(((struct jrpc_connection *) (ctx->data))->fd, mode, strlen(mode));
	}
	
	return NULL;
}

 */

/*
cJSON *seczone_modeconf_set(jrpc_context *ctx, cJSON *params, cJSON *id) {
	if (access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	
	cJSON *mode = NULL;
	cJSON *value = NULL;
	
	if (params != NULL && params->type == cJSON_Object) {
		mode = cJSON_GetObjectItem(params, "mode");
		if (mode != NULL && mode->type == cJSON_Number) {
			
			value = cJSON_GetObjectItem(params, "value");
			if (value != NULL && value->type == cJSON_String)
				strcpy(g_seczone_mode_conf[mode->valueint], value->valuestring);
		}
	}
	//write to file
	pthread_mutex_lock(&seczone_mutex);
	g_seczone_change = 1;
	pthread_mutex_unlock(&seczone_mutex);
	
	//write_seczone_conf_file();
	parse_secmode_to_seczone(g_seczone_mode);
	
	return NULL;
}

cJSON *seczone_modeconf_get(jrpc_context *ctx, cJSON *params, cJSON *id) {
	if (access_check(ctx) == 0)
		return cJSON_CreateString(ACCESS_PERMISSION_DENIED);
	
	cJSON *mode = NULL;
	char value[256];
	bzero(value, 256);
	
	if (params != NULL && params->type == cJSON_Object) {
		mode = cJSON_GetObjectItem(params, "mode");
		if (mode != NULL && mode->type == cJSON_Number) {
			snprintf(value, 256, "{\"result\":\"seczone_mode_conf\",\"mode\":\"%d\",\"value\":\"%s\"}^", mode->valueint,
					 g_seczone_mode_conf[mode->valueint]);
			app_client_t *procer = get_appt_by_ctx(ctx);
			if (procer)
				write(((struct jrpc_connection *) (ctx->data))->fd, value, strlen(value));
		}
	} else {
		
		snprintf(value, 256, "{\"result\":\"seczone_mode_conf\",\"value\":\"%s,%s,%s,%s\"}^", g_seczone_mode_conf[0],
				 g_seczone_mode_conf[1], g_seczone_mode_conf[2], g_seczone_mode_conf[3]);
		app_client_t *procer = get_appt_by_ctx(ctx);
		if (procer)
			write(((struct jrpc_connection *) (ctx->data))->fd, value, strlen(value));
	}
	
	return NULL;
}


void send_alarm_to_property(int port, char *name, char *msg) {
	char timestr[64];
	char seczone[64];
	memset(timestr, 0, 64);
	memset(seczone, 0, 64);
	char msgtoproperty[1024];
	bzero(msgtoproperty, 1024);
	
	get_time_now(timestr);
	snprintf(seczone, 64, "防区:%d-%s-%s", port, name, msg);

	GET_LOCAL_ADDRESS();
//	seczonehistory_log_to_db(timestr,"报警",seczone,message);
	
	//scott 2018.12.3
//	snprintf(msgtoproperty,1024,"{\"method\":\"emergency\",\"params\":{\"time\":\"%s\",\"ipaddr\":\"%s\",\"seczone\":\"%s\"}}",timestr,g_baseconf.outerinterfaceip,seczone);
	snprintf(msgtoproperty, 1024,
			 "{\"method\":\"emergency\",\"params\":{\"time\":\"%s\",\"ipaddr\":\"%s\",\"seczone\":\"%s\"}}", timestr,
			 g_baseconf.innerinterfaceip, seczone);
	int ret = send_msg(g_baseconf.alarmcenterip, msgtoproperty, PROPERTY_PORT);
	//log to database
	if (ret == 1) {
		seczonehistory_log_to_db(timestr, "报警#已发送", seczone, msg);
	} else {
		seczonehistory_log_to_db(timestr, "报警#发送失败", seczone, msg);
		time_t now;
		time(&now);
		if (port != 999) {
			if (now - g_seczone_conf[port - 1].lastfailtime > 10)
				create_worker(secmsg_save, msgtoproperty);
			g_seczone_conf[port - 1].lastfailtime = now;
			g_secmsg_fail_time = now;
		} else
			create_worker(secmsg_save, msgtoproperty);
	}
}

*/

void SecZoneGuard::check_nurse_time()
{
//	int i = 0,j=0;
//	time_t now = 0;
//	char cmdstr[1024];
//
//	for(i=0;i<g_seczone_count;i++) {
//		if(g_seczone_conf[i].triggertype == 3 && g_seczone_conf[i].nursetime > 0) {
//			time(&now);
//			if(g_last_nursetime == 0)
//				continue;
////			DEBUG_INFO("$$$$$$ nurse:%d %d",now,g_last_nursetime);
//			if(now-g_last_nursetime > g_seczone_conf[i].nursetime) {
//				send_emergency_per10s(i);
//			}
//		}
//	}
	
}


void SecZoneGuard::get_gpio_status() {
	
	int i;
	am335x_gpio_arg arg[8];
	am335x_gpio_arg arg_oe;
	//gpio总开关
	int fd = -1;
	int Alarm_data[8] = {0};
	time_t now;
	//这个数组保存的是上层应用设置的报警电平
	//1 表示高电平触发报警，0 表示低电平触发报警
	fd = ::open(DEV_GPIO, O_RDWR);
	if (fd < 0) {
//		DEBUG_ERR_LOG("Error device open fail! %d", fd);
		return ;
	}
	//初始化gpio
	//for(i=0;i<g_seczone_count;i++){
	for(i=0;i<8;i++){
		if(i<4) {
			arg[i].pin = GPIO_TO_PIN('E', i+14);
		}
		else {
			arg[i].pin = GPIO_TO_PIN('E', i - 4);
		}
		ioctl(fd, IOCTL_GPIO_SETINPUT, &arg[i]);
	}
	
	//打开gpio总开关
	arg_oe.pin = GPIO_TO_PIN('E', 11);
	ioctl(fd, IOCTL_GPIO_SETOUTPUT, &arg_oe);
	arg_oe.data = 0;
	ioctl(fd, IOCTL_GPIO_SETVALUE, &arg_oe);
//	DEBUG_INFO("begin to read gpio");
	char cmdstr[1024];
	int j = 0;
	while(1) {
		for(i=0;i<g_seczone_count;i++){
			//迈冲版本的GPIO口顺序是反的
			ioctl(fd, IOCTL_GPIO_GETVALUE, &arg[i]);
			if(arg[i].data == g_seczone_conf[7-i].gpiolevel) {
				generate_emergency(7-i);
				
			} else {
				
				g_seczone_conf[7-i].delaycount = 0;
			}
		}

		check_nurse_time();
//		thread_sleep(1);
	}
	::close(fd);
	
}


//每10秒种报一次警
void SecZoneGuard::send_emergency_per10s(int gpio_port)
{
//	char cmdstr[1024];
//	bzero(cmdstr,1024);
//	time_t now;
//	time(&now);
//	char msg[64];
//	bzero(msg,64);
//	switch (g_seczone_conf[gpio_port].triggertype) {
//		case 0:
//			strncpy(msg,"瞬时报警",64);
//			break;
//		case 1:
//			strncpy(msg,"延时报警",64);
//			break;
//		case 3:
//			strncpy(msg,"看护报警",64);
//			break;
//
//	}
//
//	if(now-g_seczone_conf[gpio_port].etime >= 10){
//		snprintf(cmdstr,1024,"{\"method\":\"seczone_emergency\",\"params\":{\"port\":%d,\"name\":\"%s\",\"message\":\"%s\"}}",gpio_port+1,g_seczone_conf[gpio_port].name,msg);
//
//		send_cmd_to_local("127.0.0.1",cmdstr);
//		g_seczone_conf[gpio_port].etime = now;
//	}
}


/*general gpio process*/
//@gpio_port port of gpio
void SecZoneGuard::generate_emergency(int gpio_port) {
	
	time_t now;
	time(&now);
	char message[64];
	bzero(message,64);
	
	switch ( EmergencyType(g_seczone_conf[gpio_port].triggertype)) {
		//瞬时报警
		case EmergencyType::Immediately :
			send_emergency_per10s(gpio_port);
			break;
			
			//延时报警
		case EmergencyType::Delay :
			g_seczone_conf[gpio_port].delaycount++;
			if(g_seczone_conf[gpio_port].delaycount >= g_seczone_conf[gpio_port].delaytime) {
				send_emergency_per10s(gpio_port);
			}
			break;
			
			//防劫持报警
		case EmergencyType::Hijack :
			//only send to alarm center
			strncpy(message,"防劫持报警",64);
//			send_alarm_to_property(gpio_port+1,g_seczone_conf[gpio_port].name,message);
			break;
			
			//看护报警
		case EmergencyType::Nurse :
			//
//			g_last_nursetime = now;
			break;
		
	}
	
}


SecZoneGuard::SecZoneGuard(){

}

bool SecZoneGuard::init(const Config & cfgs){
	sec_cfgs_file_  = "seczone.txt";
	load_configs();
	return true;
}

bool SecZoneGuard::open(){
	
	std::thread thread(std::bind(&SecZoneGuard::get_gpio_status,this));
	return true;
}

void SecZoneGuard::close(){

}

std::string SecZoneGuard::seczonePassword(){
	return sec_cfgs_.get_string("password","1234");
}

bool SecZoneGuard::setPassword(const std::string& old,const std::string& _new ){
	if( seczone_settings_.seczone_password != old){
		return false;
	}
	seczone_settings_.seczone_password = _new ;
	save_configs();
	return true;
}

//设置防区参数
void SecZoneGuard::setSecZoneParams(const seczone_conf_t& conf,const std::string& passwd){
	
	int n;
	for(n=0;n<seczone_settings_.seczone_confs.size();n++){
		auto & _ = seczone_settings_.seczone_confs[n];
		if(_.port == conf.port){
			_ = conf;
			break;
		}
	}
	save_configs();
}

Json::Value SecZoneGuard::getSecZoneParams(){
	return Json::Value();
}

void SecZoneGuard::onekeySet(const std::string& passwd,bool onoff){

}

//停止报警
// 通知报警服务中心，停止报警提示
void SecZoneGuard::discardEmergency(const std::string& passwd){

}

void SecZoneGuard::save_configs() {
	sec_cfgs_.save(sec_cfgs_file_);
	int n=0;
	Config cfgs;
	std::ofstream ofs(sec_cfgs_file_);
	
	ofs<<"password = "<< seczone_settings_.seczone_password << std::endl;
	ofs<<"onekeyset = "<<seczone_settings_.onekeyset << std::endl;
	
	ofs<<std::endl;
	
	ofs<<"mode.num = " << seczone_settings_.seczone_mode_confs.size() << std::endl;
	ofs<<"mode.index = "<< seczone_settings_.seczone_mode_index << std::endl;
	
	for(n=0;n< seczone_settings_.seczone_mode_confs.size();n++){
		auto _ = seczone_settings_.seczone_mode_confs[n];
		ofs<< "mode"<<_.mode <<".value = " << _.value << std::endl;
	}
	ofs<<std::endl;
	
	ofs<<"zone.num = " << seczone_settings_.seczone_confs.size() << std::endl;
	for(n=0;n< seczone_settings_.seczone_confs.size();n++){
		auto _ = seczone_settings_.seczone_confs[n];
		ofs<< "zone"<<n+1<<".port = " << _.port << std::endl;
		ofs<< "zone"<<n+1<<".name = " << _.name << std::endl;
		ofs<< "zone"<<n+1<<".normalstate = " << _.normalstate << std::endl;
		ofs<< "zone"<<n+1<<".onekeyset = " << _.onekeyset << std::endl;
		ofs<< "zone"<<n+1<<".currentstate = " << _.currentstate << std::endl;
		ofs<< "zone"<<n+1<<".delaytime = " << _.delaytime << std::endl;
		ofs<< "zone"<<n+1<<".nursetime = " << _.nursetime << std::endl;
		ofs<< "zone"<<n+1<<".alltime = " << _.alltime << std::endl;
		ofs<< "zone"<<n+1<<".gpiolevel = " << _.gpiolevel << std::endl;
		ofs<< "zone"<<n+1<<".triggertype = " << _.triggertype << std::endl;
		ofs<< "zone"<<n+1<<".online = " << _.online << std::endl;
	}
	ofs<<std::endl;
	ofs.close();
}

bool SecZoneGuard::load_configs(){
	sec_cfgs_.load( sec_cfgs_file_);
	int n;
	int num;
	std::string name;
	seczone_settings_.seczone_password = sec_cfgs_.get_string("password");
	seczone_settings_.onekeyset = sec_cfgs_.get_string("onekeyset");
	num = sec_cfgs_.get_int("mode.num");
	for(n=0;n<num;n++){
		seczone_settings_.seczone_mode_confs[n].mode = n+1;
		boost::format fmt("mode%d");
		fmt%(n+1);
		name =fmt.str();
		seczone_settings_.seczone_mode_confs[n].value = sec_cfgs_.get_string(name );
	}
	
	seczone_settings_.seczone_mode_index = sec_cfgs_.get_int("mode.index");
	
	num = sec_cfgs_.get_int("zone.num");
	
	for(n=0;n<num;n++){
		seczone_conf_t& zone = seczone_settings_.seczone_confs[n];
		name = (boost::format("zone%d.")%(n+1)).str();
		zone.port = sec_cfgs_.get_int(name+"port");
		zone.name = sec_cfgs_.get_string(name+"name");
		zone.normalstate = sec_cfgs_.get_string(name+"normalstate");
		zone.onekeyset = sec_cfgs_.get_string(name+"onekeyset");
		zone.currentstate = sec_cfgs_.get_string(name+"currentstate");
		zone.delaytime = sec_cfgs_.get_int(name+"delaytime");
		zone.nursetime = sec_cfgs_.get_int(name+"nursetime");
		zone.alltime = sec_cfgs_.get_string(name+"alltime");
		zone.gpiolevel = sec_cfgs_.get_int(name+"gpiolevel");
		zone.triggertype = sec_cfgs_.get_int(name+"triggertype");
		zone.online = sec_cfgs_.get_string(name+"online");
		
		if(zone.normalstate == "no"){
			zone.gpiolevel = 0;
		}
		if(zone.normalstate == "nc"){
			zone.gpiolevel = 1;
		}
	}
	
	return true;
}

void SecZoneGuard::init_configs(){
	int n;
	for(n=0;n<ZONE_NUMBER;n++){
	
	}
}

void SecZoneGuard::setSecModeValue(int mode,const std::string& value){
	for(auto & _ : seczone_settings_.seczone_mode_confs){
		if(_.mode == mode){
			_.value = value;
			break;
		}
	}
	save_configs();
	
}

Json::Value SecZoneGuard::getSecModeList(){
	Json::Value root;
	Json::Value array;
	
	for(auto _ : seczone_settings_.seczone_mode_confs){
		Json::Value item;
		item["mode"] = _.mode;
		item["value"] = _.value;
//		item["active"] = _.active;
		array.append(item);
	}
	return array;
}


int SecZoneGuard::getCurrentMode(){
	return seczone_settings_.seczone_mode_index;
}

void SecZoneGuard::setCurrentMode(int mode){
	seczone_settings_.seczone_mode_index = mode;
	save_configs();
}

Json::Value SecZoneGuard::getSecHistoryList(){
	return Json::Value();
}


// 本地持久化报警信息
void SecZoneGuard::save_emergency(const MessageEmergency& message){

}

