/*
 * parse2db.c
 *
 * 1/28/13
 * Baker Hamilton, M.D.
 * 
 * use with accompanying qserv.js to set up an http server that will generate
 * random questions for you  
 *
 * WARNING: not rigorously tested, although its worked for the ~1000 questions
 * I've run through it so far.
 *
 * All questions & answers should be separated from each other by a blank line
 * Format of question txt file should have chocies A through D, with each line
 * starting with a question number, text, or answer choice letter:
 * 1. This is a question
 * A. choice one
 * B. choice two
 * which can run onto a new line
 * C. choice three
 * D. choice four
 * 
 * The answers should just start with a number, and it will read them as a whole
 * 1. The answer is C
 * with explanation going here
 *
 * Image file name should just be the number of the question it pertains to with
 * the extension, ie 234.png would be read & inserted into quiz.db under question
 * 234. It only reads one file at a time, so to batch the process, put the db file
 * into the directory with all of the image files and run something like:
 *
 * $ ls *.png|awk '{print("./p2db -i "$0)}'|sh
 * 
 * If you aren't using png files or the name quiz.db, just be sure to change those
 * in qserv.js
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <sqlite3.h>

#define MAX_LENGTH 1024 // max line length
#define MAX_CHOICES 4   // choices 'A' through 'D'
#define DB_FILE "quiz.db"

typedef struct {
	int number;
	int question;
	char *text;
	char *choices[MAX_CHOICES];
} element;

element *parse(FILE *fp);
int process_imgfile(FILE *fp, char *filename, sqlite3 *db);
int process_textfile(FILE *fp, int qora, sqlite3 *db);
void usage(char *program_name);

int main(int argc, char *argv[])
{
	FILE *fp;
	sqlite3 *db;
	char *err;
	
	if (argc != 3) {
		usage(argv[0]);
		return 1;
	}

	fp = fopen(argv[2], "r");
	if (fp == NULL) {
		fprintf(stderr, "%s\n", strerror(errno));
		return 1;
	}
	
	if (sqlite3_open(DB_FILE, &db) != SQLITE_OK) {
		fprintf(stderr, "sqlite3_open(): %s\n", sqlite3_errmsg(db));
		return 1;
	}

	if (sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS quiz (qnumber INTEGER PRIMARY KEY, question TEXT, choiceA TEXT, choiceB TEXT, choiceC TEXT, choiceD TEXT, image BLOB, anumber INTEGER, answer TEXT)", NULL, NULL, &err) != SQLITE_OK) {
		fprintf(stderr, "sqlite3_exec(): %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}

	if (argv[1][1] == 'q')
		process_textfile(fp, 0, db);
	else if (argv[1][1] == 'a') 
		process_textfile(fp, 1, db);
	else if (argv[1][1] == 'i')
		process_imgfile(fp, argv[2], db);
	else {
		usage(argv[0]);
		return 1;
	}
	
	printf("\nfinished.\n");
	sqlite3_close(db);
	fclose(fp);
	
	return 0;
}

int process_textfile(FILE *fp, int qora, sqlite3 *db)
{
	element *e;
	sqlite3_stmt *stmt;
	int i;

	while ((e=parse(fp)) != NULL) {
		if (!(e->number)) // returned a blank line
			continue;

		if (!qora) { // we have a question
			fprintf(stderr, "inserting question #%d...\r", e->number);

    			if (sqlite3_prepare(db, "INSERT INTO quiz(qnumber, question, choiceA, choiceB, choiceC, choiceD) VALUES (?, ?, ?, ?, ?, ?)", -1, &stmt, 0) != SQLITE_OK) {
       				fprintf(stderr, "sqlite_prepare(): %s\n", sqlite3_errmsg(db));
       				return 1;
    			}

			if (sqlite3_bind_int(stmt, 1, e->number) != SQLITE_OK) {
				fprintf(stderr, "sqlite3_bind_int(): %s\n", sqlite3_errmsg(db));
				return 1;
			}

			if (sqlite3_bind_text(stmt, 2, e->text, strlen(e->text)+1, SQLITE_STATIC) != SQLITE_OK) {
				fprintf(stderr, "sqlite3_bind_text(): %s\n", sqlite3_errmsg(db));
				return 1;
			}
	
			for (i = 3; i < MAX_CHOICES+3; i++) // we've set the parameters for our table to support 4 choices
				// bind statements 3 through 6 with choices[0] through choices[3]
				if (sqlite3_bind_text(stmt, i, e->choices[i-3], strlen(e->choices[i-3])+1, SQLITE_STATIC) != SQLITE_OK) {
					fprintf(stderr, "sqlite3_bind_text(): %s\n", sqlite3_errmsg(db));
					return 1;
				}

		} else { // we have an answer
			fprintf(stderr, "inserting answer #%d...\r", e->number);

    			if (sqlite3_prepare(db, "UPDATE quiz SET anumber=?, answer=? WHERE qnumber=?", -1, &stmt, 0) != SQLITE_OK) {
       				fprintf(stderr, "sqlite3_prepare(): %s\n", sqlite3_errmsg(db));
       				return 1;
    			}

			if (sqlite3_bind_int(stmt, 1, e->number) != SQLITE_OK) {
				fprintf(stderr, "sqlite3_bind_int(): %s\n", sqlite3_errmsg(db));	
				return 1;
			}
	
			if (sqlite3_bind_text(stmt, 2, e->text, strlen(e->text)+1, SQLITE_STATIC) != SQLITE_OK) {
				fprintf(stderr, "sqlite3_bind_text(): %s\n", sqlite3_errmsg(db));
				return 1;
			}

			if (sqlite3_bind_int(stmt, 3, e->number) != SQLITE_OK) {
				fprintf(stderr, "sqlite3_bind_int(): %s\n", sqlite3_errmsg(db));	
				return 1;
			}
		}

		if (sqlite3_step(stmt) != SQLITE_DONE) {
			fprintf(stderr, "sqlite3_step(): %s\n", sqlite3_errmsg(db));
			return 1;
		}	

		sqlite3_reset(stmt);
	}
	
	free(e);
	return 0;
}	

int process_imgfile(FILE *fp, char *filename, sqlite3 *db)
{
	sqlite3_stmt *stmt;
	char *blob;
	int c, ino;
	long size;

	for (c = ino = 0; c < strlen(filename); c++)
		if (!isdigit(filename[c])) {
			filename[c] = '\0';
		}

	ino = atoi(filename);
	printf("entering image #%d...", ino);

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);	

	blob = calloc(size, 1);
	if (fread(blob, size, 1, fp) == 0) {
		fprintf(stderr, "fread(): error\n");
		return 1;
	}
		
	if (sqlite3_prepare(db, "UPDATE quiz SET image=? WHERE qnumber=?", -1, &stmt, 0) != SQLITE_OK) {
		fprintf(stderr, "sqlite3_prepare(): %s\n", sqlite3_errmsg(db));
		return 1;
	}

	if (sqlite3_bind_blob(stmt, 1, blob, size, SQLITE_STATIC) != SQLITE_OK) {
		fprintf(stderr, "sqlite3_bind_blob(): %s\n", sqlite3_errmsg(db));
		return 1;
	}
	
	if (sqlite3_bind_int(stmt, 2, ino) != SQLITE_OK) {
		fprintf(stderr, "sqlite3_bind_int(): %s\n", sqlite3_errmsg(db));
		return 1;
	}
	
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		fprintf(stderr, "sqlite3_step(): %s\n", sqlite3_errmsg(db));
		return 1;
	}	

	sqlite3_reset(stmt);
	
	free(blob);
	return 0;	
}

element *parse(FILE *fp)
{
	element *e = calloc(1, sizeof(element));
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int n, num, eflag = 0;
	static int prevnum = 0;
	static char letter = 'A';

	e->question = 0;
	e->text = calloc(1, sizeof(char)*MAX_LENGTH);
	for (n = 0; n < MAX_CHOICES; n++)
		e->choices[n] = calloc(1, sizeof(char)*MAX_LENGTH);

	while ((read = getline(&line, &len, fp)) != -1)	{
		num = 0;
		// if blank, we've reached the end of an element (question or answer); return it
		if (strncmp(line, "\n", MAX_LENGTH) == 0)
			return e;

		// check for a new element
		for (n = 0; isdigit(line[n]); n++) {
			if (num)
				num *= 10;

			num += line[n] - '0';
		}

		if (prevnum+1 == num) {
			eflag = 1;
			prevnum = num; // we (probably) have a new element
			e->number = num;
			
			strncpy(e->text, line, read);
		} else if (isalpha(line[0]) && line[1] == '.') { // we prob. have a new answer choice
			e->question = 1;
			switch (toupper(line[0])) {
				case 'A':
					eflag = 0; // question is over, turn off the flag
					letter = 'A';
					strncpy(e->choices[0], line, read);
					break;
				case 'B':
					letter = 'B';
					strncpy(e->choices[1], line, read);
					break;
				case 'C':
					letter = 'C';
					strncpy(e->choices[2], line, read);
					break;
				case 'D':
					letter = 'D';
					strncpy(e->choices[3], line, read);
					break;
			}
		} else { // we're in the middle of either the element body or an answer choice
			if (eflag) {
				eflag++;
				e->text = realloc(e->text, sizeof(char)*MAX_LENGTH*eflag);
				strncat(e->text, line, read);
			} else {
				switch (letter) {
					case 'A':	
						strncat(e->choices[0], line, read);
						break;
					case 'B':
						strncat(e->choices[1], line, read);
						break;
					case 'C':
						strncat(e->choices[2], line, read);
						break;
					case 'D':
						strncat(e->choices[3], line, read);
						break;
				}
			}
		}
	}
	
	if (e->number)
		return e;
	
	return NULL; 
}

void usage(char *program_name)
{
	fprintf(stderr, "Usage: %s <-q|a|i> <input file>\n", program_name);
	return;
}
