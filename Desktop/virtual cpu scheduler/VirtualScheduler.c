#define _GNU_SOURCE
#include<unistd.h>
#include<pthread.h>
#include<signal.h>
#include<stdlib.h>
#include<stdio.h>
#include<ucontext.h>
int Nb_VCPU=2;
pthread_t timer;
pthread_t mainthread;
int vcpu_quantum=3;
int Nb_Uthreads=4;
pthread_t VCPUs[5];

ucontext_t Uthreads_array[100];
ucontext_t Uthreads_per_VCPU[100][100];

pthread_t VCPU_thread_id[100];
int first_alarm_sounded;
int nb_rows;
pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;

int rows_of_current_running_ucontext[100];
void *startimer(void *i){
    while(1){int running=1;
        sleep(vcpu_quantum);
        pthread_kill(mainthread,SIGUSR1);
    
}}
void uthread_function(int i){
    while(1){
        printf("%d\n",i);
        sleep(1);
    }
}

void createUthreads(){

    for(int i=0;i<Nb_Uthreads;i++){
        
        getcontext(&Uthreads_array[i]);
        Uthreads_array[i].uc_link=0;
        if((Uthreads_array[i].uc_stack.ss_sp=(char *)malloc(4096))!=NULL){
            //used 4096 octets of memory because 1024 created a buffer overflow error
            Uthreads_array[i].uc_stack.ss_size=4096;
            Uthreads_array[i].uc_stack.ss_flags=0;
            makecontext(&Uthreads_array[i],uthread_function,1,i+1);

        }
        else{
            printf("insufficient memory");
        }
    }
    printf("successfuly created all uthreads");
}
int find_column_of_thread(pthread_t thread){
    
    for(int i=0;i<Nb_VCPU;i++){
        if(pthread_equal(thread,VCPU_thread_id[i])!=0){
            return i;
        }
    }
}
ucontext_t mainctx[100];
void turnoff(){
    pthread_t p=pthread_self();
    //get column for the current executing thread;
    int col_nb=find_column_of_thread(p);
    printf("i recieved the signal %ld and i am at column %d  first alarm %d\n",p,col_nb,first_alarm_sounded);
    if(first_alarm_sounded==1){
        //we remove current thread ,initilise the array of rows_of_current_running ucontext
        for(int i=0;i<Nb_VCPU;i++){
            rows_of_current_running_ucontext[i]=i;
            //to allow all threads intialize their current ucontext
        }
        sleep(2);
        first_alarm_sounded=2;
        ///change change with context at rows of current running ucontext
        printf("next row %d\n",rows_of_current_running_ucontext[col_nb]);
        swapcontext(&mainctx[col_nb],&Uthreads_per_VCPU[rows_of_current_running_ucontext[col_nb]][col_nb]);
    }else{
        int previous_row=rows_of_current_running_ucontext[col_nb];//store previous context
        printf("previoud row %d",previous_row);
        //increment row pointing to next uthread to run      
        rows_of_current_running_ucontext[col_nb]+=1;
        //calculate current positoin
        int position=(rows_of_current_running_ucontext[col_nb])*Nb_VCPU+(col_nb+1);
        //if there is an overflow in the matrix,
       
        if(position>Nb_Uthreads){
            //initialize cnext running ucontext row to zero
            rows_of_current_running_ucontext[col_nb]=0;
        }
        printf(" next row %d\n",rows_of_current_running_ucontext[col_nb]);
        
      swapcontext(&Uthreads_per_VCPU[previous_row][col_nb],
        &Uthreads_per_VCPU[rows_of_current_running_ucontext[col_nb]][col_nb]);
    }
    

    //////
    //executing uthreads


}



//create a lock to allow one thread run at a time when printing
void display_vcpu(int *j){
    //create a lock to store the id of each thread
    pthread_mutex_lock(&lock);
    pthread_t thread_id=pthread_self();
    VCPU_thread_id[*j]=thread_id;
    sleep(1);
    printf("VCPU no %d    %ld\n",*j,thread_id);
    printf("VCPU no %d    %ld\n",*j,thread_id);
    printf("VCPU no %d    %ld\n",*j,thread_id);
    pthread_mutex_unlock(&lock);
}


void *vcpu_function(void * i){
   //signal handler for executing vcpu scheduling
    struct sigaction act={0};
    act.sa_handler=&turnoff;
    sigaction(SIGUSR2,&act,NULL);
    printf("receiving signal");
    //signal handler for stopping printing of thread

    int *j=(int *)i;

   display_vcpu(j);
    while(1){
        
    }
    
    
}
void createVCPU(){
  /*   struct sigaction act={0};
    act.sa_handler=&turnoff;
    sigaction(SIGUSR1,&act,NULL);*/

    for(int i=0;i<Nb_VCPU;i++){ 
   
        int *a =(int *)malloc(sizeof(int));
        *a=i;
        pthread_create(&VCPUs[i],NULL,vcpu_function,a);
        sleep(1);//this sleep is to allow the execution of the get thread id of the current thread before creating other threads
    }
    //stopping vcpu
    
    //pthread_kill(VCPUs[0],SIGUSR1);//i can kill all threads with one thread since running is a shared resource
    
}



void preempt(){
    if(first_alarm_sounded==0){
        first_alarm_sounded+=1;
    }
   printf("recieved signla\n");
    for(int i=0;i<Nb_VCPU;i++){
        pthread_kill(VCPUs[i],SIGUSR2);//sending signal to this vcpu to run its program
    }
}
void dispatch_Uthreads(){
    int row=0;
    for(int i=0;i<Nb_Uthreads;i++){
        row=(i-i%Nb_VCPU)/Nb_VCPU;
        Uthreads_per_VCPU[row][i%Nb_VCPU]=Uthreads_array[i];
        
    }
    printf("successfuly dispatched all uthreads\n");
}

int main(){
     //creating vcpu
    createVCPU();
    createUthreads();
   dispatch_Uthreads();

    //linking alarm with main thread
  
    mainthread=pthread_self();
    struct sigaction alarm_sounded={0};//set all flags to zero
    alarm_sounded.sa_flags=SA_RESTART;//set restart flag so it can alwasy capture alarm signal
    alarm_sounded.sa_handler=&preempt;
    sigaction(SIGUSR1,&alarm_sounded,NULL);

    pthread_create(&timer,NULL,startimer,NULL);


   

    //we make sure the main thread keeps running as long as timer keeps running

    pthread_join(timer,NULL);
 
}