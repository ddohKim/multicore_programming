/*
 * echoserveri.c - An iterative echo server
 */
/* $begin echoserverimain */
#include "csapp.h"

void echo(int connfd);

typedef struct
{  int *buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;

sbuf_t sbuf;
void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}
void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear) % (sp->n)] = item;
    V(&sp->mutex);
    V(&sp->items);
}
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);
    P(&sp->mutex);
    item = sp->buf[(++sp->front) % (sp->n)];
    V(&sp->mutex);
    V(&sp->slots);
    return item;
}




typedef struct _item
{
    int ID;
    int left_stock;
    int price;
    int readcnt;
    sem_t mutex;
    sem_t w_lock;
    struct _item *l;
    struct _item *r;
} items;

items *root;

void show(items *root, char buf[])
{
    char pbuf[MAXLINE];
    if (root == NULL)
        return;
    // printf("%d %d %d\n",root->ID,root->left_stock,root->price);

    P(&root->mutex);
    root->readcnt++;
    if (root->readcnt == 1)
    {
        P(&root->w_lock);
    }
    V(&root->mutex);
    sprintf(pbuf, "%d %d %d\n", root->ID, root->left_stock, root->price);
    strcat(buf, pbuf);
    // printf("%s",buf);
    P(&root->mutex);
    root->readcnt--;
    if (root->readcnt == 0)
    {
        V(&root->w_lock);
    }
    V(&root->mutex);

    show(root->l, buf);
    show(root->r, buf);
}
void writeTxt(FILE *fp, items *root)
{
    if (root == NULL)
        return;
    writeTxt(fp, root->r);
    fprintf(fp, "%d %d %d\n", root->ID, root->left_stock, root->price);
    writeTxt(fp, root->l);
}
void freeNode(items *root)
{
    if (root == NULL)
        return;
    freeNode(root->r);
    freeNode(root->l);
    free(root);
}
items *putNode(int id, int left_stock, int price, items *root)
{
    if (root != NULL)
    {
        if (root->ID > id)
        {
            root->l = putNode(id, left_stock, price, root->l);
        }
        else
        {
            root->r = putNode(id, left_stock, price, root->r);
        }
    }
    else
    {
        root = (items *)malloc(sizeof(items));
        root->ID = id;
        root->price = price;
        root->left_stock = left_stock;
        root->l = NULL;
        root->r = NULL;
        sem_init(&root->mutex, 0, 1);
        sem_init(&root->w_lock, 0, 1);
        root->readcnt = 0;
    }
    return root;
}
void action(int connfd)
{
    int n;
    char buf[MAXLINE], cmd[20];
    int id, count;
    rio_t rio;
    rio_readinitb(&rio, connfd);

    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        char buf2[MAXLINE];
        memset(buf2,0,sizeof(char)*MAXLINE);
        // cmd[0] = '\0'; id = 0; price = 0;
        sscanf(buf, "%s %d %d", cmd, &id, &count);
        
        printf("server received %d bytes(%d)\n", n, connfd);
        if (!strcmp(cmd, "show"))
        {
            
            show(root, buf2);
            int slen = strlen(buf2);
            buf2[slen] = '\0';

            Rio_writen(connfd, buf2, MAXLINE);
        }
        else if (!strcmp(cmd, "exit"))
        {
            strcpy(buf2, "exit\n");
            //Close(connfd);
            printf("Disconnected to %d\n", connfd);
            return;
        }
        else if (!strcmp(cmd, "buy"))
        {
            items *temp = root;

            while (temp != NULL && temp->ID != id)
            {
                // printf("%d",temp->ID);
                if (temp->ID <= id)
                {
                    temp = temp->r;
                }
                else
                    temp = temp->l;
            }
            if(temp==NULL){
                 strcpy(buf2, "Not enough left stock\n");
                Rio_writen(connfd, buf2, MAXLINE);
                continue;
            }
            P(&temp->mutex);
            temp->readcnt++;
            if (temp->readcnt == 1)
            {
                P(&temp->w_lock);
            }
            V(&temp->mutex);
            if (temp->left_stock >= count)
            {

                P(&temp->mutex);
                temp->readcnt--;
                if (temp->readcnt == 0)
                {
                    V(&temp->w_lock);
                }
                V(&temp->mutex);

                P(&temp->w_lock);
                temp->left_stock = temp->left_stock - count;
                V(&temp->w_lock);

                strcpy(buf2, "[buy] success\n");
                Rio_writen(connfd, buf2, MAXLINE);
            }
            else
            {
                P(&temp->mutex);
                temp->readcnt--;
                if (temp->readcnt == 0)
                {
                    V(&temp->w_lock);
                }
                V(&temp->mutex);
                strcpy(buf2, "Not enough left stock\n");
                Rio_writen(connfd, buf2, MAXLINE);
            }
        }
        else if (!strcmp(cmd, "sell"))
        {
            items *temp = root;
            while (temp != NULL && temp->ID != id)
            {
                // printf("%d",temp->ID);
                if (temp->ID <= id)
                {
                    temp = temp->r;
                }
                else
                    temp = temp->l;
            }
            if(temp==NULL){
                 strcpy(buf2, "No exist stock\n");
                Rio_writen(connfd, buf2, MAXLINE);
                continue;
            }
                P(&temp->w_lock);

            temp->left_stock = temp->left_stock + count;
             V(&temp->w_lock);
            strcpy(buf2, "[sell] success\n");
            Rio_writen(connfd, buf2, MAXLINE);
           
        }
    }
}

void *thread(void *vargp)
{
        Pthread_detach(pthread_self());
    while (1)
    {
        int connfd = sbuf_remove(&sbuf);
        action(connfd);
        Close(connfd);
    }
}

void sigint_handler()
{
    FILE *fp = fopen("stock.txt", "w");
    if (fp != NULL)
    {
        writeTxt(fp, root);
        freeNode(root);
    }
    else
    {
        printf("stock.txt open error\n");
        exit(1);
    }
    fclose(fp);
    exit(0);
}
int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];
    Signal(SIGINT, sigint_handler);
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    FILE *fp = fopen("stock.txt", "r");
    if (fp != NULL)
    {
        int id, left_stock, price;
        while (fscanf(fp, "%d %d %d\n", &id, &left_stock, &price) != EOF)
        {
            //  printf("%d",id);
            root = putNode(id, left_stock, price, root);
        }
    }
    else
    {
        printf("No stock.txt\n");
        return 1;
    }
    fclose(fp);
    listenfd = Open_listenfd(argv[1]);

    pthread_t tid;
    sbuf_init(&sbuf, 500);

    for (int i = 0; i < 499; i++)
    {
        Pthread_create(&tid, NULL, thread, NULL);
    }
    while (1)
    {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        sbuf_insert(&sbuf, connfd);
    }
    exit(0);
}


