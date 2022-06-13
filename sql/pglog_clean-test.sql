--CREATE EXTENSION
CREATE EXTENSION pg_cron;
--create a job, the argument to the clean_pg_log function sets a number of days to keep the log
SELECT cron.schedule('Keep a log for 2 days and clean it every 2 minutes','*/2 * * * *', $$select cron.clean_pg_log(2)$$);

select * from cron.job;

select * from cron.job_run_details;
--View cleaning log
cat /home/postgresql14_2/pg14/data/pglog_clean/pglog_clean.log
