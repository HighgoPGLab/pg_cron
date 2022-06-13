/*-------------------------------------------------------------------------
 *
 * src/pglog_clean.c
 *
 * Clean up run logs for PostgreSQL.
 * Wording:
 *	   - Clean up run logs for PostgreSQL according to keep days
 *
 * Copyright (c) 2022, HighgoPGLab, Inc.
 *
 *-------------------------------------------------------------------------
 */
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>

#include "postgres.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "storage/fd.h"
#include "postmaster/syslogger.h"
#include "sys/time.h"
#include "time.h"

#define ERROR_MSG_LOGGING_COLLECTOR_OFF "The configuration parameter logging_collector is off, please set it to on."
#define ERROR_MSG_LOG_DIRECTORY_CHECK "The log directory of the database does not exist, please check the value of the log_directory parameter."
#define ERROR_MSG_LOG_DIRECTORY_OPEN "Failed to open the log directory of the database, please check the value of the log_directory parameter." 
#define CLEAN_LOG_DIR_NAME "pglog_clean"
#define CLEAN_LOG_FILE_NAME "pglog_clean.log"
#define ERROR_MSG_LOG_KEEP_DAYS_NULL "logkeepdays can not be NULL"
#define ERROR_MSG_LOG_KEEP_DAYS_ZERO "logkeepdays must be greater than 0"
#define CONST_LOG_KEEP_UNIT_DAY "day"

/* SQL-callable functions */
PG_FUNCTION_INFO_V1(clean_pglog);
static time_t convert_to_time(struct tm *tm_ltz);

/*
* Clean up run logs for PostgreSQL according to keep days
*/
Datum
clean_pglog(PG_FUNCTION_ARGS)
{
	time_t timep;
	struct tm *p;
	struct stat fstat;
	char pg_log_full_dir[255];
	int now_time_days;
	int file_modify_time;
	struct tm *fmt;
	int time_divisor;
	DIR *dp = NULL;
	struct dirent * fileinfo = NULL;
	unsigned int clean_cnt = 0;
	char filePath[128] = {0};
	char clean_log_full_dir[255];
	char clean_log_file_dir[255];
	char cleantime[25];//YYYY-mm-dd HH:MM:SS
	FILE *fp = NULL;
	int log_keep_days;

	/*Get the configuration information of the database parameters*/
	bool config_logging_collector = Logging_collector;
	char *config_log_directory = Log_directory;
	//char *config_log_filename = Log_filename;
	char *config_data_directory = DataDir;

	/*Check whether the parameters of PG run log are enabled*/
	if (!config_logging_collector)
	{
		ereport(ERROR, (errmsg(ERROR_MSG_LOGGING_COLLECTOR_OFF)));
	}

	/*Check pg log retention days*/
	if (PG_ARGISNULL(0))
		ereport(ERROR, (errmsg(ERROR_MSG_LOG_KEEP_DAYS_NULL)));
	else
		log_keep_days = PG_GETARG_INT32(0);

	if (log_keep_days <= 0)
		ereport(ERROR, (errmsg(ERROR_MSG_LOG_KEEP_DAYS_ZERO)));

	/*Check whether the value of Log_directory parameter is a relative path or an absolute path.*/
	if (((char*)memchr(config_log_directory, '/', 1)) == NULL)
	{
		/*In the case of relative paths, stitch with the data directory to achieve a full path*/
		sprintf(pg_log_full_dir, "%s%s%s%s", config_data_directory, "/", config_log_directory, "/");
	} else
	{
		/*The case of absolute paths*/
		sprintf(pg_log_full_dir, "%s%s", config_log_directory, "/");
	}

	/*Check PG Run Log Directory*/
	if (access(pg_log_full_dir, F_OK) == -1)
	{
		/*When it does not exist*/
		ereport(ERROR, (errmsg("%s: %s", ERROR_MSG_LOG_DIRECTORY_CHECK, pg_log_full_dir)));
	}

	/*Open the PG log directory and return the handle*/
	dp = opendir(pg_log_full_dir);
	if (NULL == dp) {
		/*open failure*/
		ereport(ERROR, (errmsg("%s: %s", ERROR_MSG_LOG_DIRECTORY_OPEN, pg_log_full_dir)));
	}

	/*Get system time*/
	time(&timep);
	p = localtime(&timep);
	sprintf(cleantime,"%04d-%02d-%02d %02d:%02d:%02d", p->tm_year + 1900, p->tm_mon + 1, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);

	time_divisor = 86400;//60*60*24
	now_time_days = (int)convert_to_time(p)/time_divisor;

	/*Set the file name for recording the cleanup log*/
	sprintf(clean_log_full_dir, "%s%s%s%s", config_data_directory, "/", CLEAN_LOG_DIR_NAME, "/");
	if (access(clean_log_full_dir, F_OK) == -1)
	{
		/*When the plug-in run log directory does not exist, create*/
		mkdir(clean_log_full_dir, S_IRWXU);
	}

	sprintf(clean_log_file_dir, "%s%s", clean_log_full_dir, CLEAN_LOG_FILE_NAME);

	/*Print PG run log cleaning process*/
	fp = fopen(clean_log_file_dir,"a+");
	fprintf(fp,"%s\n", "***PostgreSQL run logs clean start***");
	fprintf(fp,"Clean time: %s\n", cleantime);
	fprintf(fp,"Keep days: %d %s\n", log_keep_days, CONST_LOG_KEEP_UNIT_DAY);
	fprintf(fp,"Cleaned log directory: %s\n", pg_log_full_dir);
	fprintf(fp,"Cleaned file information:\n");

	/*Start cycling through the log directory*/
	while (1) {
		fileinfo = readdir(dp);
		if(fileinfo != NULL)
		{
			/*Check file type*/
			if (fileinfo->d_type == DT_REG)
			{
				//Regular Documents
			} else
			{
				//In the case of non-files (e.g. directories), skip
				continue;
			}
		} else
		{
			break;
		}

		/*Get the latest status of the file*/
		sprintf(filePath, "%s%s", pg_log_full_dir, fileinfo->d_name);
		if (stat(filePath, &fstat) == -1) {
			perror("stat");
			continue;
		}

		fmt = localtime(&fstat.st_mtime);
		file_modify_time = (int)convert_to_time(fmt)/time_divisor;
		if ((now_time_days - file_modify_time) >= log_keep_days) {
			/*Clean up log files that meet the conditions*/
			if (remove(filePath) == 0)
			{
				fprintf(fp,"[%s] Last file modification: %s",fileinfo->d_name, ctime(&fstat.st_mtime));
				clean_cnt++;
			}else
			{
				perror("remove");
				continue;
			}
		}
	};
	//Close the file handle
	closedir(dp);

	fprintf(fp,"Cleaned file number: %d\n", clean_cnt);
	fprintf(fp,"%s\n", "***PostgreSQL run logs clean completed***");

	fclose(fp);
	PG_RETURN_BOOL(true);
}

/*
* Convert year, month and day to seconds of duration since January 1, 1970
*/
static time_t
convert_to_time(struct tm *tm_ltz)
{
	struct tm info = {0};
	info.tm_year = tm_ltz->tm_year;
	info.tm_mon = tm_ltz->tm_mon;
	info.tm_mday = tm_ltz->tm_mday;
	return mktime(&info);
}