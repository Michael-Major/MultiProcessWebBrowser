#include "wrapper.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_TAB 100
extern int errno;
int pipe_status = 0;

/*
 * Name:                uri_entered_cb
 * Input arguments:     'entry'-address bar where the url was entered
 *                      'data'-auxiliary data sent along with the event
 * Output arguments:    none
 * Function:            When the user hits the enter after entering the url
 *                      in the address bar, 'activate' event is generated
 *                      for the Widget Entry, for which 'uri_entered_cb'
 *                      callback is called. Controller-tab captures this event
 *                      and sends the browsing request to the ROUTER (parent)
 *                      process.
 */
void uri_entered_cb(GtkWidget* entry, gpointer data) {
	if(data == NULL) {
		return;
	}
	browser_window *b_window = (browser_window *)data;
	// This channel has pipes to communicate with ROUTER.
	comm_channel channel = b_window->channel;
	// Get the tab index where the URL is to be rendered
	int tab_index = query_tab_id_for_request(entry, data);
	if(tab_index <= 0) {
		fprintf(stderr, "Invalid tab index (%d).", tab_index);
		return;
	}
	// Get the URL.
	char * uri = get_entered_uri(entry);
	new_uri_req pre_package;
	pre_package.render_in_tab = tab_index;
	strcpy(pre_package.uri , uri);
	child_request ch_req;
	ch_req.uri_req = pre_package;
	// Append your code here
	// ------------------------------------
	// * Prepare a NEW_URI_ENTERED packet to send to ROUTER (parent) process.
	child_req_to_parent package = {NEW_URI_ENTERED, ch_req};
	// * Send the url and tab index to ROUTER
	int wrmsg = write(channel.child_to_parent_fd[1], &package, sizeof(child_req_to_parent));

	if(wrmsg == -1)printf("Error writing to router_process in uri_entered_cb\n");
	// ------------------------------------
}

/*
 * Name:                create_new_tab_cb
 * Input arguments:     'button' - whose click generated this callback
 *                      'data' - auxillary data passed along for handling
 *                      this event.
 * Output arguments:    none
 * Function:            This is the callback function for the 'create_new_tab'
 *                      event which is generated when the user clicks the '+'
 *                      button in the controller-tab. The controller-tab
 *                      redirects the request to the ROUTER (parent) process
 *                      which then creates a new child process for creating
 *                      and managing this new tab.
 */
void create_new_tab_cb(GtkButton *button, gpointer data) {
	if(data == NULL) {
		return;
	}
	// This channel has pipes to communicate with ROUTER.
	comm_channel channel = ((browser_window*)data)->channel;
	// Append your code here.
	// ------------------------------------
	// * Send a CREATE_TAB message to ROUTER (parent) process.
	child_req_to_parent package = {0};
	package.type = CREATE_TAB;

	int wrmsg = write(channel.child_to_parent_fd[1], &package, sizeof(package));
	if(wrmsg == -1)printf("Error writing to router_process in create_new_tab_cb\n");
	// ------------------------------------
}

/*
 * Name:                url_rendering_process
 * Input arguments:     'tab_index': URL-RENDERING tab index
 *                      'channel': Includes pipes to communctaion with
 *                      Router process
 * Output arguments:    none
 * Function:            This function will make a URL-RENDRERING tab Note.
 *                      You need to use below functions to handle tab event.
 *                      1. process_all_gtk_events();
 *                      2. process_single_gtk_event();
*/
int url_rendering_process(int tab_index, comm_channel *channel) {
	// Don't forget to close pipe fds which are unused by this process
	browser_window * b_window = NULL;
	// Create url-rendering window
	create_browser(URL_RENDERING_TAB, tab_index, G_CALLBACK(create_new_tab_cb), G_CALLBACK(uri_entered_cb), &b_window, channel);
	child_req_to_parent req;
	while (1) {
		// Handle one gtk event, you don't need to change it nor understand what it does.
		process_single_gtk_event();
		// Poll message from ROUTER
		// It is unnecessary to poll requests unstoppably, that will consume too much CPU time
		// Sleep some time, e.g. 1 ms, and render CPU to other processes
		// Append your code here
		// Try to read data sent from ROUTER

		int r = read(channel->parent_to_child_fd[0], &req, sizeof(child_req_to_parent));
		// If no data being read, go back to the while loop
		// Otherwise, check message type:
		if(r == -1 && errno == EAGAIN){
			usleep(1000);
		}else if(r > 0){
			//   * NEW_URI_ENTERED
			if(req.type == NEW_URI_ENTERED){
				//     ** call render_web_page_in_tab(req.req.uri_req.uri, b_window);
				render_web_page_in_tab(req.req.uri_req.uri, b_window);
			}
			//   * TAB_KILLED
			else if(req.type == TAB_KILLED){
				//     ** call process_all_gtk_events() to process all gtk events and jump out of the loop		
				process_all_gtk_events();
				exit(0);
				break;
			}
			//   * CREATE_TAB or unknown message type
			else{
				//     ** print an error message and ignore it
				printf("CREATE_TAB or unknown message type received by URL_RENDERING_PROCESS \n");
			}
		}
		// Handle read error, e.g. what if the ROUTER has quit unexpected?
		else {
			perror("Error reading from pipe during URL_RENDERING_PROCESS\n");
			exit(-1);
		}
	}
	return 0;
}
/*
 * Name:                controller_process
 * Input arguments:     'channel': Includes pipes to communctaion with
 *                      Router process
 * Output arguments:    none
 * Function:            This function will make a CONTROLLER window and
 *                      be blocked until the program terminates.
 */
int controller_process(comm_channel *channel) {
	// Do not need to change code in this function
	pipe_status = close(channel->child_to_parent_fd[0]);

	pipe_status = close(channel->parent_to_child_fd[1]);

	browser_window * b_window = NULL;
	// Create controller window
	create_browser(CONTROLLER_TAB, 0, G_CALLBACK(create_new_tab_cb), G_CALLBACK(uri_entered_cb), &b_window, channel);
	show_browser();
	return 0;
}

/*
 * Name:                router_process
 * Input arguments:     none
 * Output arguments:    none
 * Function:            This function implements the logic of ROUTER process.
 *                      It will first create the CONTROLLER process  and then
 *                      polling request messages from all ite child processes.
 *                      It does not need to call any gtk library function.
 */
int router_process() {
	comm_channel *channel[MAX_TAB];
	int tab_pid_array[MAX_TAB] = {0}; // You can use this array to save the pid
	                                  // of every child process that has been
					  // created. When a chile process receives
					  // a TAB_KILLED message, you can call waitpid()
					  // to check if the process is indeed completed.
					  // This prevents the child process to become
					  // Zombie process. Another usage of this array
					  // is for bookkeeping which tab index has been
					  // taken.
	// Append your code here
	int i,j,r;


	channel[0] = (comm_channel*)malloc(sizeof(comm_channel));
	pipe_status = pipe(channel[0]->parent_to_child_fd);
	if(pipe_status == -1)printf("Error opening pipe for controller_process\n");
	pipe_status = pipe(channel[0]->child_to_parent_fd);
	if(pipe_status == -1)printf("Error opening pipe for controller_process\n");

	// Prepare communication pipes with the CONTROLLER process
	int pid = fork();
	// Fork the CONTROLLER process
	if(pid == 0){
		controller_process(channel[0]);
		//   call controller_process() in the forked CONTROLLER process
	}else if(pid > 0){ //parent process

		tab_pid_array[0] = pid;

		pipe_status = close(channel[0]->child_to_parent_fd[1]);

		pipe_status = close(channel[0]->parent_to_child_fd[0]);

		// Don't forget to close pipe fds which are unused by this process

		fcntl(channel[0]->child_to_parent_fd[0],F_SETFL,
			fcntl(channel[0]->child_to_parent_fd[0],F_GETFL) | O_NONBLOCK); //setting pipes to non-blocking
			
		child_req_to_parent req;

		while(1){

		// Poll child processes' communication channels using non-blocking pipes.
		// Before any other URL-RENDERING process is created, CONTROLLER process
		// is the only child process. When one or more URL-RENDERING processes
		// are created, you would also need to poll their communication pipe.

			for(i = 0 ; i < MAX_TAB ; i++){

				if(tab_pid_array[i] != 0){
					r = read(channel[i]->child_to_parent_fd[0], &req , sizeof(child_req_to_parent));
					
					if(r == -1 && errno == EAGAIN){ 
						usleep(1000);
						//   * sleep some time if no message received


					}else if(r > 0){

						//   * if message received, handle it:
						if(req.type == CREATE_TAB){
							//     ** CREATE_TAB:

							for(j = 1 ; j < MAX_TAB ; j++){ 
								if(tab_pid_array[j] == 0){
									break;
								}
							}

							if(j == MAX_TAB){ //reached max amt of tabs
								printf("Cannot create new tab:Index out of bound!\n");

							}else{

								channel[j] = (comm_channel*)malloc(sizeof(comm_channel));
								pipe_status = pipe(channel[j]->parent_to_child_fd);
								if(pipe_status == -1)printf("Error opening pipe for url_rendering_process\n");
								pipe_status = pipe(channel[j]->child_to_parent_fd);
								if(pipe_status == -1)printf("Error opening pipe for url_rendering_process\n");
								//setting up communication between processes

								int tab_pid = fork(); //forking the url_rendering_process

								if(tab_pid == -1){ // error encountered
									printf("error forking URL-RENDERING process!\n");
								}else if(tab_pid == 0){ //child process
									pipe_status = close(channel[j]->child_to_parent_fd[0]); 
									pipe_status = close(channel[j]->parent_to_child_fd[1]);
										//closing the read side of the pipe from child->parent && the write side of the pipe from parent->child
		
									fcntl(channel[j]->parent_to_child_fd[0],F_SETFL,
										fcntl(channel[j]->parent_to_child_fd[0],F_GETFL) | O_NONBLOCK);
										// Setting the pipe's writing side to nonblocking so it can communicate dynamically
									url_rendering_process(j,channel[j]);

								}else{ //parent process
									tab_pid_array[j] = tab_pid;

									pipe_status = close(channel[j]->child_to_parent_fd[1]);

									pipe_status = close(channel[j]->parent_to_child_fd[0]);
										//closing the write side of the pipe from child->parent && the read side of the pipe from parent->child

									fcntl(channel[j]->child_to_parent_fd[0],F_SETFL,
										fcntl(channel[j]->child_to_parent_fd[0],F_GETFL) | O_NONBLOCK);
										//setting the pipe's write to nonblocking
								}
							}
							//        Prepare communication pipes with the new URL-RENDERING process
							//        Fork the new URL-RENDERING process
							//
						}else if(req.type == NEW_URI_ENTERED){
							//     ** NEW_URI_ENTERED:
							int uri_index = req.req.uri_req.render_in_tab;

								//Handling errors in user input
							if(uri_index <= 0 || uri_index >= MAX_TAB){
								printf("Invalid Tab index!\n");
							}else if(tab_pid_array[uri_index] == 0){
								printf("Cannot navigate to a url in a tab that doesn't exist!\n");
							}else{
								//write(channel[uri_index]->parent_to_child_fd[1], &package, sizeof(child_req_to_parent));
								int wrmsg = write(channel[uri_index]->parent_to_child_fd[1], &req, sizeof(child_req_to_parent));
								if(wrmsg == -1)printf("Error writing to URL-RENDERING process for NEW_URI_ENTERED\n");
							}
							//        Send TAB_KILLED message to the URL-RENDERING process in which
							//        the new url is going to be rendered
							//
						}else if(req.type == TAB_KILLED){
								//     ** TAB_KILLED:
								if(i == 0){ //the controller process was killed, need to kill all tabs

									for(j = 1 ; j < MAX_TAB ; j++){
										if(tab_pid_array[j] != 0){

											int wrmsg = write(channel[j]->parent_to_child_fd[1], &req, sizeof(child_req_to_parent));
											if(wrmsg == -1)printf("Error writing to URL-RENDERING process for TAB_KILLED\n");
												//sending the message
											int status;
											waitpid(tab_pid_array[j], &status, WNOHANG);

											close(channel[j]->child_to_parent_fd[0]);
											close(channel[j]->parent_to_child_fd[1]);
												//closing the unused pipes
											tab_pid_array[j] = 0;
										}
									}
									exit(0);
								//        If the killed process is the CONTROLLER process
								//        *** send TAB_KILLED messages to kill all the URL-RENDERING processes
								//        *** call waitpid on every child URL-RENDERING processes
								//        *** self exit
								//
								}else{ //single tab was killed, closing that tab only
									int wrmsg = write(channel[i]->parent_to_child_fd[1], &req, sizeof(child_req_to_parent));
									if(wrmsg == -1)printf("Error writing to URL-RENDERING process for TAB_KILLED\n");
										//sending the message

									int status = 0;

									waitpid(tab_pid_array[i], &status, WNOHANG);
									
									pipe_status = close(channel[i]->child_to_parent_fd[0]);
									pipe_status = close(channel[i]->parent_to_child_fd[1]);
										//closing the unused pipes
									tab_pid_array[i] = 0;
								}
								//        If the killed process is a URL-RENDERING process
								//        *** send TAB_KILLED to the URL-RENDERING
								//        *** call waitpid on every child URL-RENDERING processes
								//        *** close pipes for that specific process
								//        *** remove its pid from tab_pid_array[]
								//
						}
					}else if(r == 0){ //error encountered 
						printf("error reading from child");
						exit(-1);
					}
				}
			}
		}
	}else{ //error encountered
		printf("Error forking CONTROLLER process!\n");
	}

	return 0;
}

int main() {
	return router_process();
}
