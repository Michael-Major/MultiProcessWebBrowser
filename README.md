CSci4061 F2016 Assignment 2

date: 10/29/16

name: Michael Major, Shannyn Telander, Chi-Cheng Chang

id: 4995102 / major072, 4978484 / telan024, 5327023 / chan1434

Group # 61

Multi-Process Web Browser


1. chan1434:
	Jason worked on the router process. Jason also worked with Mike on fixing the main bug that we found in our program with the pipe communication when the parent process would attempt to read from the child process when multiple tabs were opened and then some, but not all, were closed. There was also an incorrect error being thrown from the child processes when the controller tab was closed and the open tabs were all exited from.  
   
   major072:
	Mike worked on the call-back functions. Mike also worked with Jason on the aforementioned bugs that were present in the program. Mike also worked on the error-handling within the functions for all of the system calls used.  
   
   telan024:
	Originally worked on the url_rendering_process function. Shannyn also worked on checking our working solution against the given solution to check for consistencies in the browser behavior & inserting comments throughout our solution. 
	
	Overall, we attempted to work as a group, especially when taking care of the errors in order to think through the solution together.


2. < How to compile the program >

	The initial call to compile the program:

		% make

	within the directory that holds the browser.c, wrapper.c, wrapper.h and Makefile files. The executable solution can then be started using the following command:

		% ./browser	

3. < How to use the program from the shell; syntax >

	The combination of the above calls will open a window titled "CONTROLLER TAB." There is a button in the controller tab window that will open another window, referred to as a "tab." You are able to open and use multiple tabs at once, though there is a maximum number of tabs that you can open. A message will print on the shell if you have reached this maximum number of tabs. In general, if there is any error with any of the processes within the program, these messages will print to the shell. 
	
	There are also two text boxes at the top of the controller tab. The URL tab on the left can be used to direct the tab to which webpage you would like to go to. Make sure to use the full URL, beginning with http://www ... otherwise the tab will not be able to navigate to the desired webpage. 
	The text box on the right lets you choose which tab you would like to open the webpage in. If you choose an invalid tab number for whatever reason, there will be a message printed to the shell that will notify you of the mistake that you have made. Otherwise, once you have typed in the tab number that you would like to load the webpage in, you must return your cursor to the URL text box to press "ENTER" and this will load the URL that you have chosen in the tab number that you have chosen. 
	As mentioned, if there are any errors with these processes, the error message will print to the shell. Otherwise, additional information from the URL loaded may also print to the shell. Overall, besides compiling and running the executable of this program, you will not need to use the shell to use the shell when using this program.  

4. < Any explicit assumptions we've made / additional info >
 	There are no other explicit assumptions that we have made. Enjoy our multi-process web browser!
