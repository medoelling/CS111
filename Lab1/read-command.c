// UCLA CS 111 Lab 1 command reading

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

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* FIXME: You may need to add #include directives, macro definitions,
   static function definitions, etc.  */

/* FIXME: Define the type 'struct command_stream' here.  This should
   complete the incomplete type declaration in command.h.  */

enum token_type
  {
    WORD_TOKEN,
    SEMICOLON_TOKEN,
    NEWLINE_TOKEN,
    PIPELINE_TOKEN,
    LESS_THAN_TOKEN,
    GREATER_THAN_TOKEN,
    OPEN_PARAN_TOKEN,
    CLOSE_PARAN_TOKEN,
    END_OF_COMMAND,
  };

struct token
{
  enum token_type type;
  char* text;
  int line_num;
};

struct token_text
{
  char* text;
  int line_num;
};

struct command_stream
{
  struct command** command_trees;
  int index;
  int cap;
};

int isValidWordChar(char c)
{
  return (c >= '0' && c <= '9')
    || (c >= 'A' && c <= 'Z')
    || (c >= 'a' && c <= 'z')
    || c == '!' || c == '%' || c == '+'
    || c == ',' || c == '-' || c == '.'
    || c == '/' || c == ':' || c == '@'
    || c == '^' || c ==  '_';
}

int isValidWord(char* word, int word_len)
{
  int i;
  for(i = 0; i < word_len; i++)
    {
      if(!isValidWordChar(word[i]))
	return 0;
    }
  return 1;
}

int isReservedWord(char* word)
{
  return !strcmp(word, "if") || !strcmp(word, "then") || !strcmp(word, "else") ||
    !strcmp(word, "while") || !strcmp(word, "do") || !strcmp(word, "done") ||
    !strcmp(word, "fi") || !strcmp(word, "until");
}

struct token** tokenization(char* buf, struct token** tokens, int* p_tree_count)
{
  int buf_len = strlen(buf);
  int cap_token = 20;
  int cap_char = 10;
  struct token_text* tokenTexts = checked_malloc(cap_token * sizeof(struct token_text));
  int wordFlag = 0, cur_line_num =1;
  int buf_index, token_index;
  int token_count = 0, k = 0;

  for(buf_index = 0; buf_index < buf_len; buf_index++)
    {
      if(buf[buf_index] == '\n')
	cur_line_num++;
      if(buf[buf_index] == ' ' || buf[buf_index] == '\t')
	{
	  if(wordFlag)
	    {
	      if(k+1 >= cap_char)
		{
		  cap_char += 5;
		  tokenTexts[token_count-1].text = checked_realloc(tokenTexts[token_count-1].text, cap_char * sizeof(char));
		}
	      tokenTexts[token_count-1].text[k+1] = '\0';
	    }
	  
	  k = 0;
	  wordFlag = 0;
	}
      else if(buf[buf_index] == '\n' || buf[buf_index] == '<' || buf[buf_index] == '>'
	      || buf[buf_index] == '|' || buf[buf_index] == ';' || buf[buf_index] == '(' || buf[buf_index] == ')')
	{
	  if(wordFlag)
	    {
	      if(k+1 >= cap_char)
		{
		  cap_char += 5;
		  tokenTexts[token_count-1].text = checked_realloc(tokenTexts[token_count-1].text, cap_char * sizeof(char));
		}
	      tokenTexts[token_count-1].text[k+1] = '\0';
	    }
	  
	  token_count++;
	  if(token_count >= cap_token)
	    {
	      cap_token += 10;
	      tokenTexts = checked_realloc(tokenTexts, cap_token * sizeof(struct token_text));
	    }
	  
	  tokenTexts[token_count-1].text = checked_malloc(cap_char * sizeof(char));
	  tokenTexts[token_count-1].text[0] = buf[buf_index];
	  tokenTexts[token_count-1].text[1] = '\0';
	  tokenTexts[token_count-1].line_num = cur_line_num;
	  k = 0;
	  wordFlag = 0;
	}
      else
	{
	  if (wordFlag)
	    {
	      k++;
	      if(k >= cap_char)
		{
		  cap_char += 5;
		  tokenTexts[token_count-1].text = checked_realloc(tokenTexts[token_count-1].text, cap_char * sizeof(char));
		}
	      tokenTexts[token_count-1].text[k] = buf[buf_index];
	    }
	  else
	    {
	      if(buf[buf_index] == '#')
		{
		  int temp_index;
		  for(temp_index = buf_index+1; (buf[temp_index] != '\n' && temp_index < buf_len); temp_index++)
		    ;
		  buf_index = temp_index-1;
		  wordFlag = 0;
		  continue;
		}
	      if(token_count >= cap_token)
		{
		  cap_token += 10;
		  tokenTexts = checked_realloc(tokenTexts, cap_token * sizeof(struct token_text));
		}
	      token_count++;
	      cap_char = 10;
	      tokenTexts[token_count-1].text = checked_malloc(cap_char * sizeof(char));
	      tokenTexts[token_count-1].text[0] = buf[buf_index];
	      tokenTexts[token_count-1].line_num = cur_line_num;
	      wordFlag = 1;
	    }
	}
    }

  if(wordFlag)
    {
      if(k+1 >= cap_char)
	{
	  cap_char += 5;
	  tokenTexts[token_count-1].text = checked_realloc(tokenTexts[token_count-1].text, cap_char * sizeof(char));
	}
      tokenTexts[token_count-1].text[k+1] = '\0';
    }

  int cmd_tree_cap = 20;
  int cmd_tree_token_cap = 20;

  int unpaired_if = 0;
  int unpaired_while_until = 0;
  int unpaired_open_paran = 0;
  int inside_simple_command = 0;
  int after_first_command = 0;

  int text_len;
  enum token_type cur_token_type;
  int cmd_tree_index = 0;
  int cmd_tree_token_index = 0;
  int end_of_command = 0;
  tokens = checked_realloc(tokens, cmd_tree_cap * sizeof(struct token*));
  tokens[0] = checked_malloc(cmd_tree_token_cap * sizeof(struct token));
  
  for(token_index = 0; token_index < token_count; token_index++)
    {
      
      text_len = strlen(tokenTexts[token_index].text);
      if(text_len == 1)
	{
	  if(token_index == 0 && tokenTexts[token_index].text[0] == '\n')
	    continue;
	  
	  if(token_index > 0)
	    if(tokenTexts[token_index].text[0] == '\n' && tokenTexts[token_index-1].text[0] == '\n')
	      {
		if(unpaired_if || unpaired_while_until || unpaired_open_paran || !after_first_command)
		  end_of_command = 0;
		else
		  end_of_command = 1;

		continue;
	      }

	  after_first_command = 1;
	  switch(tokenTexts[token_index].text[0])
	    {
	    case ';':
	      cur_token_type = SEMICOLON_TOKEN;
	      inside_simple_command = 0;
	      if(token_index+1 == token_count || tokenTexts[token_index+1].text[0] == '\n')
		continue;
	      break;
	    case '\n':
	      cur_token_type = NEWLINE_TOKEN;
	      inside_simple_command = 0;
	      break;
	    case '|':
	      cur_token_type = PIPELINE_TOKEN;
	      inside_simple_command = 0;
	      break;
	    case '<':
	      cur_token_type = LESS_THAN_TOKEN;
	      break;
	    case '>':
	      cur_token_type = GREATER_THAN_TOKEN;
	      break;
	    case '(':
	      cur_token_type = OPEN_PARAN_TOKEN;
	      unpaired_open_paran++;
	      break;
	    case ')':
	      cur_token_type = CLOSE_PARAN_TOKEN;
	      unpaired_open_paran--;
	      if(unpaired_open_paran < 0)
		error(2,0,"Line %d: Syntax Error\n", tokenTexts[token_index].line_num);
	      break;
	    default:
	      if(isValidWordChar(tokenTexts[token_index].text[0]))
		cur_token_type = WORD_TOKEN;
	      else
		error(1,0,"Invalid Word: %c\n", tokenTexts[token_index].text[0]);
	      break;
	    }
	}
      else if(text_len > 1)
	{
	  if(isValidWord(tokenTexts[token_index].text, text_len))
	    cur_token_type = WORD_TOKEN;
	  else
	    error(1,0,"Invalid Word: %s/n", tokenTexts[token_index].text);

	  if(!isReservedWord(tokenTexts[token_index].text))
	     inside_simple_command = 1;

	  if(!inside_simple_command){
	    if(!strcmp(tokenTexts[token_index].text, "if"))
	      unpaired_if++;
	    else if(!strcmp(tokenTexts[token_index].text, "fi"))
	      unpaired_if--;
	    else if(!strcmp(tokenTexts[token_index].text, "while"))
	      unpaired_while_until++;
	    else if(!strcmp(tokenTexts[token_index].text, "until"))
	      unpaired_while_until++;
	    else if(!strcmp(tokenTexts[token_index].text, "done"))
	      unpaired_while_until--;

	    if(unpaired_if < 0 || unpaired_while_until < 0)
	      error(2,0,"Line %d: Syntax Error\n", tokenTexts[token_index].line_num);
	  }
	}
 
      if(end_of_command)
	{
	  switch(cur_token_type)
	    {
	    case LESS_THAN_TOKEN:
	    case GREATER_THAN_TOKEN:
	    case PIPELINE_TOKEN:
	    case SEMICOLON_TOKEN:
	      error(1,0,"Line %d: Syntax error: Invalid char at start of line!!!\n", tokenTexts[token_index].line_num);
	    break;
	    case WORD_TOKEN:
	    case OPEN_PARAN_TOKEN:
	    case CLOSE_PARAN_TOKEN:
	      /*cmd_tree_token_index++;
	      if(cmd_tree_token_index >= cmd_tree_token_cap)
		{
		  cmd_tree_token_cap += 1;
		  tokens[cmd_tree_index] = (struct token*) checked_realloc(tokens[cmd_tree_index], cmd_tree_token_cap * sizeof(struct token));
		  }*/
		 
	      tokens[cmd_tree_index][cmd_tree_token_index-1].text = 0;
	      tokens[cmd_tree_index][cmd_tree_token_index-1].type = END_OF_COMMAND;
	      tokens[cmd_tree_index][cmd_tree_token_index-1].line_num = tokenTexts[token_index-1].line_num;
	      
	      cmd_tree_token_cap = 20;
	      cmd_tree_index++;
	       if(cmd_tree_index >= cmd_tree_cap)
		 {
		   cmd_tree_cap += 10;
		   tokens = checked_realloc(tokens, cmd_tree_cap * sizeof(struct token*));
		 }
	      tokens[cmd_tree_index] = checked_malloc(cmd_tree_token_cap * sizeof(struct token));
	      cmd_tree_token_index = 0;
	      end_of_command = 0;
	      break;
	    default:
	      error(1,0,"Syntax error!!!\n");
	      break;
	    }
	}

      if(cmd_tree_token_index >= cmd_tree_token_cap)
	{
	  cmd_tree_token_cap += 10;
	  tokens[cmd_tree_index] = checked_realloc(tokens[cmd_tree_index], cmd_tree_token_cap * sizeof(struct token));
	}
      tokens[cmd_tree_index][cmd_tree_token_index].text = tokenTexts[token_index].text;
      tokens[cmd_tree_index][cmd_tree_token_index].type = cur_token_type;
      tokens[cmd_tree_index][cmd_tree_token_index].line_num = tokenTexts[token_index].line_num;
      cmd_tree_token_index++;
    }
  if(cur_token_type != NEWLINE_TOKEN)
    {
      cmd_tree_token_index++;
      if(cmd_tree_token_index >= cmd_tree_token_cap)
	{
	  cmd_tree_token_cap++;
	  tokens[cmd_tree_index] = checked_realloc(tokens[cmd_tree_index], cmd_tree_token_cap * sizeof(struct token));
	}
    }
  tokens[cmd_tree_index][cmd_tree_token_index-1].text = 0;
  tokens[cmd_tree_index][cmd_tree_token_index-1].type = END_OF_COMMAND;
  tokens[cmd_tree_index][cmd_tree_token_index-1].line_num = tokenTexts[token_index-1].line_num;
  *p_tree_count = cmd_tree_index+1;

  free(tokenTexts);
  
  return tokens;
}

void test_tokenization(struct token** tokens, int tree_count)
{
  FILE* fp;
  fp = fopen("test_tokenization.out", "w");
  int i;
  int j;
  
  for (i = 0; i < tree_count ;i++)
    {
      for(j = 0; tokens[i][j].type != END_OF_COMMAND; j++)
	fprintf(fp, "** %s ** %d\n", tokens[i][j].text, tokens[i][j].line_num);
      fprintf(fp, "==== END OF COMMAND ====\n");
    }
  fclose(fp);
}

void tokens_to_words(struct token* tokens, char** words)
{
  int i;
  int words_cap = 10;
  //words =  checked_realloc(words, words_cap * sizeof(char*));
  
  for(i = 0; tokens[i].type != END_OF_COMMAND; i++)
    {
      if(i >= words_cap)
	{
	  words_cap += 10;
	  words = (char**) checked_realloc(words, words_cap * sizeof(char*));
	}
      words[i] = tokens[i].text;
    }
  words[i] = 0;
  //return words;
}

void set_input(struct command* cmd, struct token* cur_token)
{
  cur_token = &cur_token[1];
  if(cur_token->type != WORD_TOKEN)
    error(1,0,"Line %d: Syntax Error: invalid input\n", cur_token->line_num);
  cmd->input = cur_token->text;

  if(cur_token[1].type == WORD_TOKEN)
    error(1,0,"Line %d: Syntax Error: invalid input\n", cur_token->line_num);
}

void set_output(struct command* cmd, struct token* cur_token)
{
  cur_token = &cur_token[1];
  if(cur_token->type != WORD_TOKEN)
    error(1,0,"Line %d: Syntax Error: invalid input\n", cur_token->line_num);
  cmd->output = cur_token->text;
  
  if(cur_token[1].type == WORD_TOKEN)
    error(1,0,"Line %d: Syntax Error: invalid input\n", cur_token->line_num);
    
}

int set_io(struct command* cmd, struct token* cur_token)
{
  if(cur_token->type == LESS_THAN_TOKEN)
    {
      set_input(cmd, cur_token);
      cur_token = &cur_token[2];
      if(cur_token->type == GREATER_THAN_TOKEN)
	{
	  set_output(cmd, cur_token);
	  return 3;
	}
      return 1;
    }
  else if(cur_token->type == GREATER_THAN_TOKEN)
    {
      set_output(cmd, cur_token);
      return 1;
    }
  return 1;
}

void copy_command(struct command* a, struct command* b)
{
  b->type = a->type; b->input = a->input; b->output = a->output; b->status = a->status;
  b->u.command[0] = a->u.command[0]; b->u.command[1] = a->u.command[1]; b->u.command[2] = a->u.command[2];
}

int open_paran_appeared = 0;
struct token* make_command_tree(struct command* cmd, struct token* tree_token)
{
  char* cur_token_text = tree_token[0].text; 
  enum token_type cur_token_type = tree_token[0].type;
  if(cur_token_type == END_OF_COMMAND)
    error(2,0,"Line %d: Syntax Error\n",tree_token[0].line_num);
  struct token* next_token = &(tree_token[1]);

  if(!cmd)
    cmd = checked_malloc(sizeof(struct command));
  cmd->input = 0; cmd->output = 0; cmd->status = -1;
  if(!strcmp(cur_token_text, "if"))
    {
      cmd->type = IF_COMMAND;
      
      if(next_token->type == END_OF_COMMAND)
	error(1,0,"Line %d: Syntax Error: incomplete if statement\n", next_token->line_num);
      if(next_token->type == NEWLINE_TOKEN)
	next_token = &(next_token[1]);
      cmd->u.command[0] = checked_malloc(sizeof(struct command));
      cmd->u.command[1] = checked_malloc(sizeof(struct command));
      next_token = make_command_tree(cmd->u.command[0], next_token);
      cmd->u.command[2] = 0;

      if(next_token->type == NEWLINE_TOKEN)
	next_token = &next_token[1];
      if( next_token->text == 0 || strcmp(next_token->text, "then") ||next_token[1].type == END_OF_COMMAND )
	error(1,0,"Line %d: Syntax Error: incomplete if statement\n", next_token->line_num);

      next_token = &next_token[1];
      if(next_token->type == NEWLINE_TOKEN)
	next_token = &(next_token[1]);
      next_token = make_command_tree(cmd->u.command[1], next_token);

      if(next_token->text == 0)
	error(1,0,"Line %d: Syntax Error: incomplete if statement\n", next_token->line_num);
      
      if(!strcmp(next_token->text, "else"))
	{
	  if(next_token[1].type == END_OF_COMMAND)
	    error(1,0,"Line %d: Syntax Error: incomplete if statement\n", next_token->line_num);
	  if(next_token[1].type == NEWLINE_TOKEN)
	    next_token = &(next_token[1]);
	  cmd->u.command[2] = checked_malloc(sizeof(struct command));
	  next_token = &next_token[1];
	  next_token = make_command_tree(cmd->u.command[2], next_token);
	  
	  if(next_token->text == 0 || strcmp(next_token->text, "fi"))
	    error(1,0,"Line %d: Syntax Error: incomplete if statement\n", next_token->line_num);
	}
      else if(next_token->text == 0 || strcmp(next_token->text, "fi"))
	error(1,0,"Line %d: Syntax Error: incomplete if statement\n", next_token->line_num);

      next_token = &next_token[1];

      if(next_token->type == LESS_THAN_TOKEN || next_token->type == GREATER_THAN_TOKEN)
	{
	  int index_incr = set_io(cmd, next_token) + 1;
	  next_token = &next_token[index_incr];
	}
      if(next_token->type == NEWLINE_TOKEN || next_token->type == SEMICOLON_TOKEN)
	next_token = &next_token[1];
      else if(next_token->type == WORD_TOKEN)
	error(1,0,"Line %d : Syntax Error: incomplete if statement\n", next_token->line_num);

      if(next_token->type == CLOSE_PARAN_TOKEN && !open_paran_appeared)
	error(1,0,"Line %d: Syntax Error: unexpected )\n", next_token->line_num);


      if(next_token->type != END_OF_COMMAND
	 && strcmp(next_token->text, "then")
	 && strcmp(next_token->text, "else")
	 && strcmp(next_token->text, "fi")
	 && strcmp(next_token->text, "done")
	 && strcmp(next_token->text, "do")
	 && next_token->type != CLOSE_PARAN_TOKEN
	  && next_token->type != NEWLINE_TOKEN)
	{
	  struct command* temp_cmd = checked_malloc(sizeof(struct command));
	  copy_command(cmd, temp_cmd);
	  cmd->type = SEQUENCE_COMMAND;
	  cmd->status = -1;
	  cmd->input = 0;
	  cmd->output = 0;
	  cmd->u.command[0] = temp_cmd;
	  cmd->u.command[1] = checked_malloc(sizeof(struct command));
	  next_token = make_command_tree(cmd->u.command[1], next_token);
	}
    }
  else if(!strcmp(cur_token_text, "while"))
    {
      cmd->type = WHILE_COMMAND;
      
      if(next_token->type == END_OF_COMMAND)
	error(1,0,"Line %d: Syntax Error: incomplete while statement\n", next_token->line_num);
      if(next_token->type == NEWLINE_TOKEN)
	next_token = &(next_token[1]);
      cmd->u.command[0] = checked_malloc(sizeof(struct command));
      cmd->u.command[1] = checked_malloc(sizeof(struct command));
      next_token = make_command_tree(cmd->u.command[0], next_token);
      
      if(next_token-> text == 0 || strcmp(next_token->text, "do") || next_token[1].type == END_OF_COMMAND)
	error(1,0,"Line %d: Syntax Error: incomplete while statement\n", next_token->line_num);

      next_token = &next_token[1];
      if(next_token->type == NEWLINE_TOKEN)
	next_token = &(next_token[1]);
      
      next_token = make_command_tree(cmd->u.command[1], next_token);
      
      if(next_token->text == 0 || strcmp(next_token->text, "done"))
	error(1,0,"Line %d: Syntax Error: incomplete while statement\n", next_token[-1].line_num);
      
      next_token = &next_token[1];

      if(next_token->type == LESS_THAN_TOKEN || next_token->type == GREATER_THAN_TOKEN)
	{
	  int index_incr = set_io(cmd, next_token) + 1;
	  next_token = &next_token[index_incr];
	}
      if(next_token->type == NEWLINE_TOKEN || next_token->type == SEMICOLON_TOKEN)
	next_token = &next_token[1];
      else if(next_token->type == WORD_TOKEN)
	error(1,0,"Line %d : Syntax Error: incomplete if statement\n", next_token->line_num);

      if(next_token->type == CLOSE_PARAN_TOKEN && !open_paran_appeared)
	error(1,0,"Line %d: Syntax Error: unexpected )\n", next_token->line_num);


      if(next_token->type != END_OF_COMMAND
	 && strcmp(next_token->text, "then")
	 && strcmp(next_token->text, "else")
	 && strcmp(next_token->text, "fi")
	 && strcmp(next_token->text, "done")
	 && strcmp(next_token->text, "do")
	 && next_token->type != CLOSE_PARAN_TOKEN
	  && next_token->type != NEWLINE_TOKEN)
	{
	  struct command* temp_cmd = checked_malloc(sizeof(struct command));
	  copy_command(cmd, temp_cmd);
	  cmd->type = SEQUENCE_COMMAND;
	  cmd->status = -1;
	  cmd->input = 0;
	  cmd->output = 0;
	  cmd->u.command[0] = temp_cmd;
	  cmd->u.command[1] = checked_malloc(sizeof(struct command));
	  next_token = make_command_tree(cmd->u.command[1], next_token);
	}
    }
  else if(!strcmp(cur_token_text, "until"))
    {
      cmd->type = UNTIL_COMMAND;
      
      if(next_token->type == END_OF_COMMAND)
	error(1,0,"Line %d: Syntax Error: incomplete until statement\n", next_token->line_num);
      if(next_token->type == NEWLINE_TOKEN)
	next_token = &(next_token[1]);
      cmd->u.command[0] = checked_malloc(sizeof(struct command));
      cmd->u.command[1] = checked_malloc(sizeof(struct command));
      next_token = make_command_tree(cmd->u.command[0], next_token);
      
      if( next_token-> text == 0 || strcmp(next_token->text, "do") || next_token[1].type == END_OF_COMMAND)
	error(1,0,"Line %d: Syntax Error: incomplete until statement\n", next_token->line_num);
      
      next_token = &next_token[1];
      if(next_token->type == NEWLINE_TOKEN)
	next_token = &(next_token[1]);
      
      next_token = make_command_tree(cmd->u.command[1], next_token);
      
      if(next_token->text == 0 || strcmp(next_token->text, "done"))
	error(1,0,"Line %d: Syntax Error: incomplete until statement\n", next_token->line_num);

      next_token = &next_token[1];

      if(next_token->type == LESS_THAN_TOKEN || next_token->type == GREATER_THAN_TOKEN)
	{
	  int index_incr = set_io(cmd, next_token) + 1;
	  next_token = &next_token[index_incr];
	}
      if(next_token->type == NEWLINE_TOKEN || next_token->type == SEMICOLON_TOKEN)
	next_token = &next_token[1];
      else if(next_token->type == WORD_TOKEN)
	error(1,0,"Line %d : Syntax Error: incomplete if statement\n", next_token->line_num);

      if(next_token->type == CLOSE_PARAN_TOKEN && !open_paran_appeared)
	error(1,0,"Line %d: Syntax Error: unexpected )\n", next_token->line_num);


      if(next_token->type != END_OF_COMMAND
	 && strcmp(next_token->text, "then")
	 && strcmp(next_token->text, "else")
	 && strcmp(next_token->text, "fi")
	 && strcmp(next_token->text, "done")
	 && strcmp(next_token->text, "do")
	 && next_token->type != CLOSE_PARAN_TOKEN
	 && next_token->type != NEWLINE_TOKEN)
	{
	  struct command* temp_cmd = checked_malloc(sizeof(struct command));
	  copy_command(cmd, temp_cmd);
	  cmd->type = SEQUENCE_COMMAND;
	  cmd->status = -1;
	  cmd->input = 0;
	  cmd->output = 0;
	  cmd->u.command[0] = temp_cmd;
	  cmd->u.command[1] = checked_malloc(sizeof(struct command));
	  next_token = make_command_tree(cmd->u.command[1], next_token);
	}
    }
  else if(cur_token_type == WORD_TOKEN)
    {
      //int temp_token_cap = 20;
      //struct token* temp_tokens = checked_malloc(temp_token_cap * sizeof(struct token));
      int cur_token_index, cur_word_index;
      int io_redirection_appeared = 0;
      int word_cap = 10;
      cmd->u.word = checked_malloc(word_cap * sizeof(char*));

      if(tree_token[0].text == 0 || !strcmp(tree_token[0].text, "do") ||!strcmp(tree_token[0].text, "done")
	 ||!strcmp(tree_token[0].text, "else") || !strcmp(tree_token[0].text, "fi"))
	error(1,0,"Line %d: Syntax Error\n",tree_token->line_num);
      
      for(cur_token_index = 0; tree_token[cur_token_index].type != END_OF_COMMAND; cur_token_index++)
	{
	  if(!io_redirection_appeared)
	    cur_word_index = cur_token_index;
	  cur_token_type = tree_token[cur_token_index].type;
	  cur_token_text = tree_token[cur_token_index].text;
	  cmd->type = SIMPLE_COMMAND;
	  if(cur_word_index >= word_cap)
	    {
	      word_cap += 10;
	      cmd->u.word = checked_realloc(cmd->u.word, word_cap * sizeof(char*));
	    }
	  cmd->u.word[cur_word_index] = cur_token_text;
	  
	  if(cur_token_type == SEMICOLON_TOKEN || cur_token_type == NEWLINE_TOKEN)
	    {
	      cur_token_index++;
	      cmd->u.word[cur_word_index] = 0;
	      cur_token_text = tree_token[cur_token_index].text;
	      cur_token_type = tree_token[cur_token_index].type;
	      
	      if( cur_token_type == END_OF_COMMAND  || !strcmp(cur_token_text, "then")
		 || !strcmp(cur_token_text, "else") || !strcmp(cur_token_text, "fi")
		 || !strcmp(cur_token_text, "done") || !strcmp(cur_token_text, "do") || cur_token_type == CLOSE_PARAN_TOKEN)
		  next_token = &(tree_token[cur_token_index]);	
	      else
		{
		  struct command* temp_cmd = checked_malloc(sizeof(struct command));
		  copy_command(cmd, temp_cmd);
		  cmd->type = SEQUENCE_COMMAND;
		  cmd->status = -1;
		  cmd->input = 0;
		  cmd->output = 0;
		  cmd->u.command[0] = temp_cmd;
		  cmd->u.command[1] = checked_malloc(sizeof(struct command));
		  next_token = &(tree_token[cur_token_index]);
		  next_token = make_command_tree(cmd->u.command[1], next_token);
		  if(next_token->type == CLOSE_PARAN_TOKEN && !open_paran_appeared)
		    error(1,0,"Line %d: Syntax Error: unexpected )\n", next_token->line_num);

		}

	      return next_token;
	    }
	  else if(cur_token_type == PIPELINE_TOKEN)
	    {
	      cmd->u.word[cur_word_index] = 0;
	      cur_token_index++;
	      cur_token_text = tree_token[cur_token_index].text;
	      cur_token_type = tree_token[cur_token_index].type;

	      if(cur_token_type == END_OF_COMMAND)
		error(1,0,"Line %d: Syntax Error: no command after |\n",tree_token[cur_token_index].line_num);

	      struct command* temp_cmd = checked_malloc(sizeof(struct command));
	      copy_command(cmd, temp_cmd);
	      cmd->type = PIPE_COMMAND; next_token = &(tree_token[cur_token_index]);
	      cmd->input = 0;
	      cmd->output = 0;
	      cmd->status = -1;

	      cmd->u.command[0] = temp_cmd;
	      cmd->u.command[1] = checked_malloc(sizeof(struct command));
	      next_token = make_command_tree(cmd->u.command[1], next_token);
	      if(next_token->type == CLOSE_PARAN_TOKEN && !open_paran_appeared)
		error(1,0,"Line %d: Syntax Error: unexpected )\n", next_token->line_num);


	      return next_token;
	    }
	  else if(cur_token_type == CLOSE_PARAN_TOKEN)
	    {
	      if(!open_paran_appeared)
		error(1,0,"Line %d: Syntax Error: unexpected )\n", tree_token[cur_token_index].line_num);
	      cmd->u.word[cur_word_index] = 0;
	      next_token = &(tree_token[cur_token_index]);
	      return next_token;
	    }
	  else if(cur_token_type == LESS_THAN_TOKEN || cur_token_type == GREATER_THAN_TOKEN)
	    {
	      cur_token_index += set_io(cmd, &tree_token[cur_token_index]);
	      io_redirection_appeared = 1;
	    }
	}
      if(!io_redirection_appeared)
	cur_word_index = cur_token_index;

      cmd->u.word[cur_word_index] = 0;
      next_token = &(tree_token[cur_token_index]);
    }
  else if(cur_token_type == OPEN_PARAN_TOKEN)
    {
      if(next_token->type == NEWLINE_TOKEN)
	next_token = &next_token[1];
      if(next_token->type != WORD_TOKEN && next_token->type != OPEN_PARAN_TOKEN)
	error(1,0,"Line %d: Syntax Error: invalid word after (\n",next_token->line_num);
      open_paran_appeared++;
      cmd->u.command[0] = checked_malloc(sizeof(struct command));
      cmd->type = SUBSHELL_COMMAND;
      cmd->status = -1;
      cmd->input = 0;
      cmd->output = 0;
      next_token = make_command_tree(cmd->u.command[0], next_token);

      if(next_token->type != CLOSE_PARAN_TOKEN)
	error(1,0,"Line %d: Syntax Error: Invalid Subshell Command!!!\n", next_token->line_num);
      open_paran_appeared--;

      next_token = &next_token[1];
      
      if(next_token->type == LESS_THAN_TOKEN || next_token->type == GREATER_THAN_TOKEN)
	{
	  int index_incr = set_io(cmd, next_token) + 1;
	  next_token = &next_token[index_incr];
	}
      if(next_token->type == NEWLINE_TOKEN || next_token->type == SEMICOLON_TOKEN)
	next_token = &next_token[1];

      if(next_token->type != END_OF_COMMAND
	 && strcmp(next_token->text, "then")
	 && strcmp(next_token->text, "else")
	 && strcmp(next_token->text, "fi")
	 && strcmp(next_token->text, "done")
	 && strcmp(next_token->text, "do")
	 && next_token->type != CLOSE_PARAN_TOKEN
	  && next_token->type != NEWLINE_TOKEN)
	{
	  struct command* temp_cmd = checked_malloc(sizeof(struct command));
	  copy_command(cmd, temp_cmd);
	  cmd->type = SEQUENCE_COMMAND;
	  cmd->status = -1;
	  cmd->input = 0;
	  cmd->output = 0;
	  cmd->u.command[0] = temp_cmd;
	  cmd->u.command[1] = checked_malloc(sizeof(struct command));
	  next_token = make_command_tree(cmd->u.command[1], next_token);
	}
    }
  else
    error(1,0,"Line %d: Syntax Error\n", tree_token->line_num);

  if(next_token->type == CLOSE_PARAN_TOKEN && !open_paran_appeared)
    error(1,0,"Line %d: Syntax Error: unexpected )\n", next_token->line_num);
  
  return next_token;
}

command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
  int char_cap = 100;
  int char_count = 0;
  char cur_char;
  struct token** script_tokens = checked_malloc(sizeof(struct token*));
  int cmd_tree_count = 0;
  struct command_stream* cmd_stream = checked_malloc(sizeof(struct command_stream));
  char* script_stream = checked_malloc(char_cap * sizeof(char));

  for(;;)
    {
      cur_char = get_next_byte(get_next_byte_argument);
      if(cur_char == EOF)
	break;

      if(char_count >= char_cap)
	{
	  char_cap += 50;
	  script_stream =  checked_realloc(script_stream, char_cap * sizeof(char));
	}

      script_stream[char_count] = cur_char;
      char_count++;
    }

  script_tokens = tokenization(script_stream, script_tokens, &cmd_tree_count);
  free(script_stream);
  //test_tokenization(script_tokens, cmd_tree_count);  //comment out when turn in !!!!!

  cmd_stream->command_trees = checked_malloc(cmd_tree_count * sizeof(struct command*));
  cmd_stream->index = 0;
  cmd_stream->cap = cmd_tree_count;
  
  int cmd_tree_index;
  for(cmd_tree_index = 0; cmd_tree_index < cmd_tree_count; cmd_tree_index++)
    {
      cmd_stream->command_trees[cmd_tree_index] = checked_malloc(sizeof(struct command));
      make_command_tree(cmd_stream->command_trees[cmd_tree_index], script_tokens[cmd_tree_index]);
    }
  return cmd_stream;
}


command_t
read_command_stream (command_stream_t s)
{
  int i;
  if(s->index < s->cap)
    {
      i = s->index;
      s->index++;
      return s->command_trees[i];
    }
  return 0;
}


