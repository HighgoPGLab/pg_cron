## What is it?
PostgreSQL can keep running logs for a fixed period such as hours, weeks, months, etc. 
If we want to keep logs for an irregular period such as 5 days, 10 days, 15 days, etc., PostgreSQL can not do it.
The newly added pglog_clean.c implements the ability to keep PG run logs for N days. For example, 5 days, 10 days, etc.
Added cleanup of PG run logs based on pg_cron. 
The native functions of pg_cron is retained, and for details, refer to https://github.com/citusdata/pg_cron.git.

## Supported versions of PostgreSQL
Tested on PostgreSQL 10.0+

## Using
```
CREATE EXTENSION pg_cron;
--create a job, the argument to the clean_pg_log function sets a number of days to keep the log
SELECT cron.schedule('Keep a log for 2 days and clean it every 2 minutes','*/2 * * * *', $$select cron.clean_pg_log(2)$$);

--File directory for recording the cleaning process
*/data/pglog_clean/pglog_clean.log
```