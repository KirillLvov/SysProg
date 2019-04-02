#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct cmd
{
	const char *name;
	const char **argv;
	int argc;
};

void print_cmd (struct cmd command)
{
	printf("Command: %s\nArguments:", command.name);
	for(int i=0; i<command.argc; i++)
		printf(" %s", command.argv[i]);
	printf("\n");
}

int main (int argc, char* argv[])
{
	struct cmd command;
	int words_num = 100;
	char **words = (char **)malloc(sizeof(char*)*words_num);
	for(int i=0; i<words_num; i++)
		words[i] = (char*)malloc(sizeof(char)*100);
	char ch;
	char buf[100];
	int count_words = 0;
	while(1) {
		printf("\n> ");
		count_words = 0;
		while(scanf("%c", &ch)) {
			if(ch == '\"') {	
				if(scanf("%[^\"]\"", buf)){
					sprintf(words[count_words], "\"%s\"", buf);
					while(*(buf + strlen(buf) - 1) == '\\') {
						if(scanf("%[^\"]\"", buf))
							sprintf(words[count_words], "%s%s\"", words[count_words], buf);
						else {
							sprintf(words[count_words], "%s\"", words[count_words]);
							scanf("\"");
							break;
						}
					}
				}
				else {
					sprintf(words[count_words], "\"\"");
					scanf("\"");
				}
				count_words ++;
			}
			else if(ch == ' ') {}
			else if(ch == '\n')
				break;
			else {
				if(scanf("%[^ \n]", buf))
					sprintf(words[count_words], "%c%s", ch, buf);
				else
					sprintf(words[count_words], "%c", ch);
				count_words ++;
			} 
		}
		for(int i=0; i<count_words; i++)
			printf("%s|", words[i]);
		//command.name = buf;
		//command.argv = (const char **)(&buf);
		//command.argc = 1;
		//print_cmd(command);
	}
}
