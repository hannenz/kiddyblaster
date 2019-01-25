#ifndef __CARD_H__
#define __CARD_H__


typedef struct {
    unsigned int id;
    char name[100];
    char uri[256];
} Card;

Card* card_read(int card_id);

#endif
