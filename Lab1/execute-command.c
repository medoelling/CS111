// UCLA CS 111 Lab 1 command execution

// Copyright 2012-2014 Paul Eggert.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#include "alloc.h"
#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

double NANO = 1000000000.0;
double MICRO = 1000000.0;
int PFD;
void execute_aux(command_t c);

int
prepare_profiling (char const *name)
{
  int fd =  open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  PFD = fd;
  return fd;
}

int
command_status (command_t c)
{
  return c->status;
}


void
log_simple_cmd(command_t c, struct timespec realtime1, struct timespec realtime2, struct timespec finishtime,
	  struct timeval usertime, struct timeval systime)
{
  if(PFD == -1) return;
  
  int oldfd = dup(1);
  if(dup2(PFD, 1) < 0)
    error(1,0,"Failed to use file as profiling output\n");
  int char_cap = 1023;
  int nchar;
  char *buf = checked_malloc(char_cap * sizeof(char));
  char *tempbuf = buf;
  double ft = (double) finishtime.tv_sec + finishtime.tv_nsec/NANO;
  double frt = (double) realtime2.tv_sec + realtime2.tv_nsec/NANO;
  double srt = (double) realtime1.tv_sec + realtime1.tv_nsec/NANO;

  double rt = frt - srt;
  double ut = (double)usertime.tv_sec + usertime.tv_usec/MICRO;
  double syst = (double)systime.tv_sec + systime.tv_usec/MICRO;

  nchar = snprintf(tempbuf, char_cap, "%f ", ft);
  char_cap -= nchar; tempbuf = tempbuf+nchar;
  
  nchar = snprintf(tempbuf, char_cap, "%f ", rt);
  char_cap -= nchar;  tempbuf = tempbuf+nchar;
  
  nchar = snprintf(tempbuf, char_cap, "%f ", ut);
  char_cap -= nchar;  tempbuf = tempbuf+nchar;
  
  nchar = snprintf(tempbuf, char_cap, "%f ", syst);
  char_cap -= nchar;  tempbuf = tempbuf+nchar;

  char **w = c->u.word;
  nchar = snprintf (tempbuf, char_cap, "%s",  *w);
  char_cap -= nchar; tempbuf = tempbuf+nchar;
  while (*++w)
    {
      if(char_cap <= 0)
	break;
      nchar = snprintf (tempbuf, char_cap, " %s", *w);
      char_cap -= nchar; tempbuf = tempbuf+nchar;
    }
  
  if (c->input)
    {
      nchar = snprintf (tempbuf, char_cap, "<%s", c->input);
      char_cap -= nchar; tempbuf = tempbuf+nchar;
    }
  
  if (c->output)
    nchar = snprintf (tempbuf, char_cap, ">%s", c->output);
  
  printf("%s\n", buf);
  fflush(stdout);
  dup2(oldfd,1);
  free(buf);
}

void
log_other_cmd(pid_t pid, struct timespec realtime1, struct timespec realtime2, struct timespec finishtime,
	  struct timeval usertime, struct timeval systime)
{
  if(PFD == -1) return;
  
  int oldfd = dup(1);
  if(dup2(PFD, 1) < 0)
    error(1,0,"Failed to use file as profiling output\n");
  int char_cap = 1023;
  int nchar;
  char *buf = checked_malloc(char_cap * sizeof(char));
  char *tempbuf = buf;
  double ft = (double) finishtime.tv_sec + finishtime.tv_nsec/NANO;
  double frt = (double) realtime2.tv_sec + realtime2.tv_nsec/NANO;
  double srt = (double) realtime1.tv_sec + realtime1.tv_nsec/NANO;

  double rt = frt - srt;
  double ut = (double)usertime.tv_sec + usertime.tv_usec/MICRO;
  double syst = (double)systime.tv_sec + systime.tv_usec/MICRO;

  nchar = snprintf(tempbuf, char_cap, "%f ", ft);
  char_cap -= nchar; tempbuf = tempbuf+nchar;
  
  nchar = snprintf(tempbuf, char_cap, "%f ", rt);
  char_cap -= nchar;  tempbuf = tempbuf+nchar;
  
  nchar = snprintf(tempbuf, char_cap, "%f ", ut);
  char_cap -= nchar;  tempbuf = tempbuf+nchar;
  
  nchar = snprintf(tempbuf, char_cap, "%f ", syst);
  char_cap -= nchar;  tempbuf = tempbuf+nchar;

  snprintf(tempbuf, char_cap, "[%d]", pid);
  printf("%s\n", buf);
  fflush(stdout);
  dup2(oldfd,1);
  free(buf);
}

void setup_io(command_t c);

void
execute_if_command(command_t c)
{
  int status;
  
  struct timespec tp1, tp2, ftp;
  if(clock_gettime(CLOCK_MONOTONIC, &tp1) == -1)
    error(1,errno, "Failed to get clock\n");

  pid_t pid = fork();
  if(pid > 0)
    {
      waitpid(pid, &status, 0);
      if(!WEXITSTATUS(status))
	{
	execute_aux(c->u.command[1]);
	c->status = c->u.command[1]->status;
	}
      else
	{
	execute_aux(c->u.command[2]);
	c->status = c->u.command[2]->status;
	}
      struct rusage ru;
      if(getrusage(RUSAGE_CHILDREN, &ru) == -1)
	error(1,errno, "Failed to get rusage\n");
      
      if(clock_gettime(CLOCK_MONOTONIC, &tp2) == -1)
	error(1,errno, "Failed to get clock\n");

      if(clock_gettime(CLOCK_REALTIME, &ftp) == -1)
	error(1,errno, "Failed to get clock\n");

      log_other_cmd(pid, tp1, tp2, ftp, ru.ru_utime, ru.ru_stime);
      
    }
  else if(pid == 0)
    {
      execute_aux(c->u.command[0]);
      exit(c->u.command[0]->status);
    }
  else
    error(2,0,"Fork failed\n");
}

void
execute_pipe_command(command_t c)
{
  int fd[2];
  pid_t pid1, pid2, rpid;
  int status;

  if(pipe(fd) == -1)
    error(2, 0, "Pipe Failed\n");

  struct rusage ru1,ru2;
  struct timespec tp11, tp12, tp21, tp22, ftp1, ftp2;
  if(clock_gettime(CLOCK_MONOTONIC, &tp11) == -1)
    error(1,errno, "Failed to get clock\n");
      
  pid1 = fork();
  if(pid1 > 0)
    {
      if(clock_gettime(CLOCK_MONOTONIC, &tp21) == -1)
	error(1,errno, "Failed to get clock\n");
     
      pid2 = fork();
      if(pid2 > 0)
	{
	  close(fd[0]); close(fd[1]);

	  rpid = waitpid(-1, &status, 0);
	  if(rpid == pid1)
	    {
	      waitpid(pid2, &status, 0);
	      c->status = status;
	    }
	  else if(rpid == pid2)
	    {
	      waitpid(pid1, &status, 0);
	      c->status = status;
	    }

	  if(getrusage(RUSAGE_CHILDREN, &ru2) == -1)
	    error(1,errno, "Failed to get rusage\n");
      
	  if(clock_gettime(CLOCK_MONOTONIC, &tp22) == -1)
	    error(1,errno, "Failed to get clock\n");

	  if(clock_gettime(CLOCK_REALTIME, &ftp2) == -1)
	    error(1,errno, "Failed to get clock\n");

	  log_other_cmd(pid2, tp21, tp22, ftp2, ru2.ru_utime, ru2.ru_stime);
	  
	}
      else if(pid2 == 0)
	{
	  close(fd[0]);
	  if(dup2(fd[1], 1) == -1)
	    error(2, 0, "Failed to write to pipe\n");
	  //close(fd[1]);
	  execute_aux(c->u.command[0]);
	  exit(c->u.command[0]->status);
	}
      else
	error(2, 0, "Failed to fork\n");
      
      if(getrusage(RUSAGE_CHILDREN, &ru1) == -1)
	error(1,errno, "Failed to get rusage\n");
      
      if(clock_gettime(CLOCK_MONOTONIC, &tp12) == -1)
	error(1,errno, "Failed to get clock\n");

      if(clock_gettime(CLOCK_REALTIME, &ftp1) == -1)
	error(1,errno, "Failed to get clock\n");

      log_other_cmd(pid1, tp11, tp12, ftp1, ru1.ru_utime, ru1.ru_stime);
    }
  else if(pid1 == 0)
    {
      close(fd[1]);
      if(dup2(fd[0], 0) == -1)
	error(2, 0, "Failed to read from pipe\n");
      close(fd[1]);
      execute_aux(c->u.command[1]);
      exit(c->u.command[1]->status);
    }
  else
    error(2,0,"fork falied\n");
}

void
execute_sequence_command(command_t c)
{
  int status;
  pid_t pid;
  struct timespec tp1, tp2, ftp;
  if(clock_gettime(CLOCK_MONOTONIC, &tp1) == -1)
    error(1,errno, "Failed to get clock\n");
  
  pid = fork();
  if(pid > 0)
    {
      waitpid(pid, &status, 0);
      execute_aux(c->u.command[1]);
      
      struct rusage ru;
      if(getrusage(RUSAGE_CHILDREN, &ru) == -1)
	error(1,errno, "Failed to get rusage\n");
      
      if(clock_gettime(CLOCK_MONOTONIC, &tp2) == -1)
	error(1,errno, "Failed to get clock\n");

      if(clock_gettime(CLOCK_REALTIME, &ftp) == -1)
	error(1,errno, "Failed to get clock\n");

      log_other_cmd(pid, tp1, tp2, ftp, ru.ru_utime, ru.ru_stime);
      c->status = c->u.command[1]->status;
    }
  else if(pid == 0)
    {
      execute_aux(c->u.command[0]);
      exit(c->u.command[0]->status);
    }
  else
    error(2,0,"fork falied\n");
}

void
execute_simple_command(command_t c)
{
  int status;
  pid_t pid;
  struct timespec tp1, tp2, ftp;
  if(clock_gettime(CLOCK_MONOTONIC, &tp1) == -1)
    error(1,errno, "Failed to get clock\n");
  pid = fork();
  if(pid > 0)
    {
      waitpid(pid, &status, 0);
      struct rusage ru;
      if(getrusage(RUSAGE_CHILDREN, &ru) == -1)
	error(1,errno, "Failed to get rusage\n");

      if(clock_gettime(CLOCK_MONOTONIC, &tp2) == -1)
	error(1,errno, "Failed to get clock\n");

      if(clock_gettime(CLOCK_REALTIME, &ftp) == -1)
	error(1,errno, "Failed to get clock\n");

      log_simple_cmd(c,tp1, tp2, ftp, ru.ru_utime, ru.ru_stime);
      c->status = WEXITSTATUS(status);
    }
  else if(pid == 0)
    {
      setup_io(c);
      execvp(c->u.word[0], c->u.word);
      error(2,0, "Failed to execute simple command");
    }
  else
    error(2,0,"fork failed\n");
}

void
execute_subshell_command(command_t c)
{
  int status;
  struct timespec tp1, tp2, ftp;
  if(clock_gettime(CLOCK_MONOTONIC, &tp1) == -1)
    error(1,errno, "Failed to get clock\n");
  
  pid_t pid = fork();
  if(pid > 0)
    {
      waitpid(pid, &status, 0);
      
      struct rusage ru;
      if(getrusage(RUSAGE_CHILDREN, &ru) == -1)
	error(1,errno, "Failed to get rusage\n");

      if(clock_gettime(CLOCK_MONOTONIC, &tp2) == -1)
	error(1,errno, "Failed to get clock\n");

      if(clock_gettime(CLOCK_REALTIME, &ftp) == -1)
	error(1,errno, "Failed to get clock\n");

      log_other_cmd(pid,tp1, tp2, ftp, ru.ru_utime, ru.ru_stime);
      
      c->status = status;
    }
  else if(pid == 0)
    {
      setup_io(c);
      execute_aux(c->u.command[0]);
      exit(c->u.command[0]->status);
    }
  else
    error(2,0,"fork failed\n");
}

void
execute_until_command(command_t c)
{
  int status;
  struct timespec tp1, tp2, ftp;
  if(clock_gettime(CLOCK_REALTIME, &tp1) == -1)
    error(1,errno, "Failed to get clock\n");
  
  while(1){
  pid_t pid = fork();
  if(pid > 0)
    {
      waitpid(pid, &status, 0);
      if(WEXITSTATUS(status))
	  execute_aux(c->u.command[1]);
      else
	break;

      struct rusage ru;
      if(getrusage(RUSAGE_CHILDREN, &ru) == -1)
	error(1,errno, "Failed to get rusage\n");

      if(clock_gettime(CLOCK_MONOTONIC, &tp2) == -1)
	error(1,errno, "Failed to get clock\n");

      if(clock_gettime(CLOCK_REALTIME, &ftp) == -1)
	error(1,errno, "Failed to get clock\n");

      log_other_cmd(pid,tp1, tp2, ftp, ru.ru_utime, ru.ru_stime);
      
      c->status = status;
    }
  else if(pid == 0)
    {
      execute_aux(c->u.command[0]);
      exit(c->u.command[0]->status);	 
    }
  else
    error(2,0,"Fork failed\n");
  }
}

void
execute_while_command(command_t c)
{
  int status;
  struct timespec tp1, tp2, ftp;
  if(clock_gettime(CLOCK_REALTIME, &tp1) == -1)
    error(1,errno, "Failed to get clock\n");
  
  while(1){
  pid_t pid = fork();
  if(pid > 0)
    {
      waitpid(pid, &status, 0);
      if(!WEXITSTATUS(status))
	  execute_aux(c->u.command[1]);
      else
	break;

      struct rusage ru;
      if(getrusage(RUSAGE_CHILDREN, &ru) == -1)
	error(1,errno, "Failed to get rusage\n");

      if(clock_gettime(CLOCK_MONOTONIC, &tp2) == -1)
	error(1,errno, "Failed to get clock\n");

      if(clock_gettime(CLOCK_REALTIME, &ftp) == -1)
	error(1,errno, "Failed to get clock\n");

      log_other_cmd(pid,tp1, tp2, ftp, ru.ru_utime, ru.ru_stime);
      
      c->status = status;
    }
  else if(pid == 0)
    {
      execute_aux(c->u.command[0]);
      exit(c->u.command[0]->status);	 
    }
  else
    error(2,0,"Fork failed\n");
  }

}

void
setup_io(command_t c)
{
  if(c->input)
    {
      int file = open(c->input, O_RDWR, 0644);
      if(file < 0)
	error(1,0,"Failed to open file:%s\n", c->input);

      if(dup2(file, 0) < 0)
	error(1,0,"Failed to use file:%s as input\n", c->input);

      if(close(file) < 0)
	error(1,0,"Failed to close file:%s\n", c->input);
    }

  if(c->output)
    {
      int file = open(c->output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if(file < 0)
	error(1,0,"Failed to open file:%s\n", c->output);

      if(dup2(file, 1) < 0)
	error(1,0,"Failed to use file:%s as output\n", c->output);

      if(close(file) < 0)
	error(2,0,"Failed to close file:%s\n", c->output);
    }
}

void
execute_aux(command_t c)
{    
  switch(c->type)
    {
    case IF_COMMAND:
      execute_if_command(c);
      break;
    case PIPE_COMMAND:
      execute_pipe_command(c);
      break;
    case SEQUENCE_COMMAND:
      execute_sequence_command(c);
      break;
    case SIMPLE_COMMAND:
      execute_simple_command(c);
      break;
    case SUBSHELL_COMMAND:
      execute_subshell_command(c);
      break;
    case UNTIL_COMMAND:
      execute_until_command(c);
      break;
    case WHILE_COMMAND:
      execute_while_command(c);
      break;
    default:
      error(2,0,"Unknown Command Type\n");
    }
}

void
execute_command (command_t c)
{ 
  execute_aux(c);
}
