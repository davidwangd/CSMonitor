# Duonao Adapter

Implement a duonao adapter for the monitor, using the standard output of comannd line for the interface provided.

## get_job_list

```bash
djob | grep RUNNING | awk '{ print $1 }'
```

will return some lines, each line stands for a job, you need to remove the previous or later space or tab if there are.

## get_job_stdout_file

```bash
djob -ll <jobid> | grep stdoutRedirectPath | awk '{ print $2 }'
```

## get_job_stderr_file

```bash
djob -ll <jobid> | grep stderrRedirectPath | awk '{ print $2 }'
```

## get_job_info

```bash
djob -ll <jobid> | grep name | awk '{ print $2 }'
```

