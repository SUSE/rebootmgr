// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <errno.h>
#include <stdarg.h>
#include <spawn.h>
#include <sys/wait.h>

#include "basics.h"
#include "exec_cmd.h"

extern char **environ;

/* Executes a command using posix_spawnp and waits for it to finish.
   If "output" is not NULL, the command's stdout is captured and
   returned as a malloc'ed, NUL-terminated string in *output, which
   the caller has to free. On failure, *output is set to NULL.
   Returns 0 on success, -errno on failure, > 0 exit code of the
   called command. */
int
exec_cmd(char **output, char *const *extra_env, const char *cmd, ...)
{
  _cleanup_free_ char **argv = NULL;
  _cleanup_free_ char **envp_buf = NULL;
  char **envp = environ;
  va_list args, args_copy;
  int argc;
  int r;
  int pipefd[2] = { -1, -1 };
  posix_spawn_file_actions_t file_actions;
  posix_spawn_file_actions_t *file_actionsp = NULL;

  if (output)
    *output = NULL;

  argc = 0;
  va_start(args, cmd);
  va_copy(args_copy, args);
  while (va_arg(args_copy, const char *) != NULL)
    argc++;
  va_end(args_copy);

  argv = malloc((argc + 1) * sizeof(char *));
  if (!argv)
    {
      va_end(args);
      return -ENOMEM;
    }

  for (int i = 0; i < argc; i++)
    argv[i] = (char *)va_arg(args, const char *);
  argv[argc] = NULL;
  va_end(args);

  if (extra_env && extra_env[0])
    {
      size_t env_count = 0;
      while (environ[env_count])
	env_count++;
      size_t extra_count = 0;
      while (extra_env[extra_count])
	extra_count++;

      envp_buf = malloc((env_count + extra_count + 1) * sizeof(char *));
      if (!envp_buf)
	return -ENOMEM;

      memcpy(envp_buf, environ, env_count * sizeof(char *));
      memcpy(envp_buf + env_count, extra_env, extra_count * sizeof(char *));
      envp_buf[env_count + extra_count] = NULL;
      envp = envp_buf;
    }

  if (output)
    {
      if (pipe(pipefd) == -1)
	return -errno;

      r = posix_spawn_file_actions_init(&file_actions);
      if (r != 0)
	{
	  close(pipefd[0]);
	  close(pipefd[1]);
	  return -r;
	}
      file_actionsp = &file_actions;

      posix_spawn_file_actions_adddup2(&file_actions, pipefd[1], STDOUT_FILENO);
      posix_spawn_file_actions_addclose(&file_actions, pipefd[0]);
      posix_spawn_file_actions_addclose(&file_actions, pipefd[1]);
    }

  pid_t pid;

  r = posix_spawnp(&pid, cmd, file_actionsp, NULL, argv, envp);

  if (file_actionsp)
    posix_spawn_file_actions_destroy(file_actionsp);

  if (r != 0)
    {
      if (output)
	{
	  close(pipefd[0]);
	  close(pipefd[1]);
	}
      return -r;
    }

  _cleanup_free_ char *buf = NULL;

  if (output)
    {
      close(pipefd[1]);

      size_t size = 0;
      size_t capacity = 0;
      ssize_t n;
      char chunk[4096];

      while ((n = read(pipefd[0], chunk, sizeof(chunk))) > 0)
	{
	  if (size + (size_t)n + 1 > capacity)
	    {
	      capacity = size + (size_t)n + 1;
	      char *tmp = realloc(buf, capacity);
	      if (!tmp)
		{
		  close(pipefd[0]);
		  waitpid(pid, NULL, 0);
		  return -ENOMEM;
		}
	      buf = tmp;
	    }
	  memcpy(buf + size, chunk, (size_t)n);
	  size += (size_t)n;
	  buf[size] = '\0';
	}

      close(pipefd[0]);

      if (!buf)
	{
	  buf = strdup("");
	  if (!buf)
	    {
	      waitpid(pid, NULL, 0);
	      return -ENOMEM;
	    }
	}
    }

  // Wait for the child process to complete
  int wait_status;
  while (waitpid(pid, &wait_status, 0) == -1)
    {
      if (errno == EINTR)
        continue;
      return -errno;
    }

  // Evaluate the exit status
  if (WIFEXITED(wait_status))
    {
      int exit_code = WEXITSTATUS(wait_status);
      if (exit_code == 0 && output)
	*output = TAKE_PTR(buf);
      if (exit_code != 0)
	return exit_code;
      return 0;
    }
  else if (WIFSIGNALED(wait_status))
    {
      int sig = WTERMSIG(wait_status);
      // Conventionally, shells return 128 + signal number for fatal signals
      return 128 + sig;
    }

  // fprintf(stderr, "Command '%s' terminated in an unknown state.\n", cmd);
  return -EIO;
}
