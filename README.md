 Structure of the program:
 My program contains three functions: parse, execute, and main. It also has 
 four arrays: buffer, tokens, arg, and redirect. When the program runs, the main 
 function prompts the user to put input and reads this input to the buffer array. 
 Then the main calls the parse function to tokenize the words in the buffer and 
 populate the arg array and redirect array. The redirect array stores any 
 redirection symbol and the file name that immediately following it (so the array 
 looks like [symbol1, file1, symbol2, file2]). The arg array stores the rest 
 of arguments. Then the main calls the execute function and passes in the  
 redirect and arg arrays. The execute function first handles the built-in shell
 commands like "cd" and "rm." Then it handles "bg" and "fg" commands, sending 
 continue signals and giving terminal control to processes accordingly.  Then 
 the function handles Input/Output redirects and executes the file. At the end, 
 the function checks the status and prints out messages accordingly.

 There aren't any known bugs.

 To compile the program with the "33sh>" prompt, run "./33sh".
 To compile the program without the "33sh>" prompt, run "./33noprompt".



