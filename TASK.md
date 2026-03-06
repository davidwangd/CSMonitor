# Duonao Adapter

Implement a duonao adapter for the monitor, using the standard output of comannd line for the interface provided.

## get_job_list

```bash
djob | tail -n +2 | awk '{ print $1 }'
```

will return some lines, each line stands for a job, you need to remove the previous or later space or tab if there are.

## get_job_stdout_file

```bash
```

