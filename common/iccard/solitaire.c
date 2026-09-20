#include "solitaire.h"

#include <memory.h>

#define DECK_SIZE 22
#define JOKER_A 21
#define JOKER_B 22

typedef struct {
    char m_Deck[DECK_SIZE];
} DECK, *PDECK;

static DECK SOL_INIT_DECK = {
    .m_Deck = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 },
};

#define char2num(c) ((c) - '0' + 1)
static inline char num2char(char num) {
    while (num < 1) num = num + 10;
    return (num - 1) % 10 + '0';
}

static void SolMoveCard(PDECK lpDeck, char card) {
    int p = 0;

    for (int i = 0; i < DECK_SIZE; i++) {
        if (lpDeck->m_Deck[i] == card) {
            p = i;
            break;
        }
    }

    if (p < DECK_SIZE - 1) {
        lpDeck->m_Deck[p] = lpDeck->m_Deck[p + 1];
        lpDeck->m_Deck[p + 1] = card;
    } else {
        for (int i = DECK_SIZE - 1; i > 1; i--) lpDeck->m_Deck[i] = lpDeck->m_Deck[i - 1];
        lpDeck->m_Deck[1] = card;
    }
}

static void SolCutDeck(PDECK lpDeck, char point) {
    DECK tmp;

    memcpy(tmp.m_Deck, &lpDeck->m_Deck[(size_t)point], DECK_SIZE - point - 1);
    memcpy(&tmp.m_Deck[DECK_SIZE - point - 1], lpDeck->m_Deck, point);
    memcpy(lpDeck->m_Deck, tmp.m_Deck, DECK_SIZE - 1);
}

static void SolSwapOutsideJoker(PDECK lpDeck) {
    int j1 = -1;
    int j2 = -1;
    DECK tmp;

    for (int i = 0; i < DECK_SIZE; i++) {
        if (lpDeck->m_Deck[i] == JOKER_A || lpDeck->m_Deck[i] == JOKER_B) {
            if (j1 == -1) {
                j1 = i;
            } else {
                j2 = i;
            }
        }
    }

    if (0 < DECK_SIZE - j2 - 1) memcpy(tmp.m_Deck, &lpDeck->m_Deck[j2 + 1], DECK_SIZE - j2 - 1);
    tmp.m_Deck[DECK_SIZE - j2 - 1] = lpDeck->m_Deck[j1];
    if (0 < j2 - j1 - 1) memcpy(&tmp.m_Deck[DECK_SIZE - j2], &lpDeck->m_Deck[j1 + 1], j2 - j1 - 1);
    tmp.m_Deck[DECK_SIZE - j1 - 1] = lpDeck->m_Deck[j2];
    if (0 < j1) memcpy(&tmp.m_Deck[DECK_SIZE - j1], lpDeck->m_Deck, j1);
    memcpy(lpDeck->m_Deck, tmp.m_Deck, DECK_SIZE);
}

static void SolCutByBottomCard(PDECK lpDeck) {
    char p = lpDeck->m_Deck[DECK_SIZE - 1];
    if (p == JOKER_B) p = JOKER_A;
    SolCutDeck(lpDeck, p);
}

static char SolGetTopCardNum(PDECK lpDeck) {
    char p = lpDeck->m_Deck[0];
    if (p == JOKER_B) p = JOKER_A;
    return lpDeck->m_Deck[(size_t)p];
}

static void SolDeckHash(PDECK lpDeck) {
    char p;

    do {
        SolMoveCard(lpDeck, JOKER_A);
        SolMoveCard(lpDeck, JOKER_B);
        SolMoveCard(lpDeck, JOKER_B);
        SolSwapOutsideJoker(lpDeck);
        SolCutByBottomCard(lpDeck);

        p = SolGetTopCardNum(lpDeck);
    } while (p == JOKER_A || p == JOKER_B);
}

static void SolCreateDeck(PDECK lpDeck, const char *key) {
    memcpy_s(lpDeck, sizeof *lpDeck, &SOL_INIT_DECK, sizeof SOL_INIT_DECK);
    int p = 0;
    while (key[p] != '\0') {
        SolDeckHash(lpDeck);
        char c = char2num(key[p]);
        SolCutDeck(lpDeck, c);
        p++;
    }
}

void SolitaireCipherEncode(const char *szKey, const char *szSrc, char *szDst) {
    DECK deck;
    SolCreateDeck(&deck, szKey);

    int i = 0;
    while (szSrc[i] != '\0') {
        SolDeckHash(&deck);
        char p = SolGetTopCardNum(&deck);
        szDst[i] = num2char(char2num(szSrc[i]) + p);
        i++;
    }
    szDst[i] = '\0';
}

void SolitaireCipherDecode(const char *szKey, const char *szSrc, char *szDst) {
    DECK deck;
    SolCreateDeck(&deck, szKey);

    int i = 0;
    while (szSrc[i] != '\0') {
        SolDeckHash(&deck);
        char p = SolGetTopCardNum(&deck);
        szDst[i] = num2char(char2num(szSrc[i]) - p);
        i++;
    }
    szDst[i] = '\0';
}

void SolitaireCipher(int nMode, const char *szKey, const char *szSrc, char *szDst) {
    if (nMode == 0)
        SolitaireCipherEncode(szKey, szSrc, szDst);
    else if (nMode == 1)
        SolitaireCipherDecode(szKey, szSrc, szDst);
}
