/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

void echo(int connfd);

int fd_num=0;

typedef struct _pool{
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pools;

 typedef struct _item{
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t r_lock;
    struct _item* l;
    struct _item* r;
} items;

items* root;

void init_pool(int listenfd, pools *p){
    p->maxi=-1;
    for(int i=0;i<FD_SETSIZE;i++){
        p->clientfd[i]=-1;
    }
    p->maxfd=listenfd;
    //printf("%d\n",listenfd);
    FD_ZERO(&p->read_set);
    FD_SET(listenfd,&p->read_set);
}
void add_client(int connfd, pools *p){
    p->nready--;
   int temp;
    for (int i = 0; i < FD_SETSIZE; i++)
      {  
          temp=i;
          if (p->clientfd[i] < 0) {
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);

            FD_SET(connfd, &p->read_set);
            
            if (connfd > p->maxfd)
                p->maxfd = connfd;
            if (i > p->maxi)
                p->maxi = i;
            break;
        }
      }
    if (temp == FD_SETSIZE) 
        app_error("add_client error: Too many clients");

    fd_num++;

}

void show(items* root,char buf[]){
    char pbuf[MAXLINE];
    if(root==NULL)
        return;
   // printf("%d %d %d\n",root->ID,root->left_stock,root->price);
    sprintf(pbuf,"%d %d %d\n",root->ID,root->left_stock,root->price);
    strcat(buf,pbuf);
   // printf("%s",buf);

   show(root->l,buf);
    show(root->r,buf);

}
void writeTxt(FILE* fp, items* root){
    if(root==NULL) return;
    writeTxt(fp,root->r);
    fprintf(fp,"%d %d %d\n",root->ID,root->left_stock,root->price);
    writeTxt(fp,root->l);
}
void freeNode(items* root){
    if(root==NULL) return;
    freeNode(root->r);
    freeNode(root->l);
    free(root);
}
items* putNode(int id, int left_stock, int price,items* root){
 if(root!=NULL){
                if(root->ID>id){
                    root->l=putNode(id,left_stock,price,root->l);
                }
                else{
                     root->r=putNode(id,left_stock,price,root->r);
                }
            }
            else{
                root=(items*)malloc(sizeof(items));
                root->ID=id;
                root->price=price;
                root->left_stock=left_stock;
                root->l=NULL;
                root->r=NULL;
            }
            return root;
}



void check_clients(pools *p){
    int connfd, n;
    char buf[MAXLINE],cmd[20];
    int id, count;
    rio_t rio;

    for (int i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];
         if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
             
            p->nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
                char buf2[MAXLINE];
                memset(buf2,0,sizeof(char)*MAXLINE);
               // cmd[0] = '\0'; id = 0; price = 0;
                sscanf(buf, "%s %d %d", cmd, &id, &count);
                // printf("%s",cmd);
                printf("server received %d bytes(%d)\n", n, connfd);
              
                if (!strcmp(cmd, "show")) {
              //  printf("hello");
                    show(root, buf2);
                   // printf("h2");
                  // printf("%s",buf2);
                    int slen = strlen(buf2);
                    buf2[slen] = '\0';
                    
                    Rio_writen(connfd, buf2, MAXLINE);
            
                }
                else if (!strcmp(cmd, "exit")) {
                    strcpy(buf2,"exit\n");
                     Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
               fd_num--;
               if(fd_num==0){

                   FILE* fp=fopen("stock.txt","w");
                   if(fp!=NULL){
                       writeTxt(fp, root);
                       //freeNode(root);
                   }
                   else{
                       printf("stock.txt open error\n");
                       exit(1);
                   }
                   fclose(fp);
               }
                   printf("Disconnected to %d\n", connfd);
    
                }
                 else if (!strcmp(cmd, "buy")) {
                   items* temp=root;
                 
                    while(temp!=NULL&&temp->ID!=id){
                         // printf("%d",temp->ID);
                        if(temp->ID<=id){
                            temp=temp->r;
                        }
                        else temp=temp->l;
                    }
                    if(temp->left_stock>=count){
                        temp->left_stock=temp->left_stock-count;
                        strcpy(buf2,"[buy] success\n");
                          Rio_writen(connfd, buf2, MAXLINE);
                    }
                    else{
                         strcpy(buf2,"Not engoufh left stock\n");
                          Rio_writen(connfd, buf2, MAXLINE);
                    }
                }
                 else if (!strcmp(cmd, "sell")) {
                    items* temp=root;
                    while(temp!=NULL&&temp->ID!=id){
                         // printf("%d",temp->ID);
                        if(temp->ID<=id){
                            temp=temp->r;
                        }
                        else temp=temp->l;
                    }
                     temp->left_stock=temp->left_stock+count;
                        strcpy(buf2,"[sell] success\n");
                          Rio_writen(connfd, buf2, MAXLINE);
                }
            }

            else {
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
               fd_num--;
               if(fd_num==0){

                   FILE* fp=fopen("stock.txt","w");
                   if(fp!=NULL){
                       writeTxt(fp, root);
                       //freeNode(root);
                   }
                   else{
                       printf("stock.txt open error\n");
                       exit(1);
                   }
                   fclose(fp);
               }
               printf("Disconnected to %d\n", connfd);
            }
        }
    }
}

void sigint_handler() { 
     FILE* fp=fopen("stock.txt","w");
                   if(fp!=NULL){
                       writeTxt(fp, root);
                       freeNode(root);
                   }
                   else{
                       printf("stock.txt open error\n");
                       exit(1);
                   }
                   fclose(fp);
    exit(1);
}
int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  
   static pools pool;
    char client_hostname[MAXLINE], client_port[MAXLINE];
   Signal(SIGINT, sigint_handler); 
    if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
    }

    FILE* fp=fopen("stock.txt","r");
    if(fp!=NULL){
        int id,left_stock,price;
        while(fscanf(fp,"%d %d %d\n",&id, &left_stock,&price)!=EOF){
        //  printf("%d",id);
           root=putNode(id,left_stock,price,root);
        }
    }
    else {
        printf("No stock.txt\n");
        return 1;}
        fclose(fp);
    listenfd = Open_listenfd(argv[1]);
    //printf("%d",listenfd);
    init_pool(listenfd,&pool);
    
       while (1) {
       
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);

     
        if (FD_ISSET(listenfd, &pool.ready_set)) {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
            Getnameinfo((SA*)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
            //printf("%d",connfd);
           // printf("%d",listenfd);
            add_client(connfd, &pool);
        }
       
   
        check_clients(&pool);
    }
    exit(0);
}



