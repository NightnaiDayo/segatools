#include <stdint.h>
#include <windows.h>

#include "util/dprintf.h"

#include "hooklib/y3.h"


uint16_t y3_io_get_api_version() {
    return 0x0100;
}

HRESULT y3_io_init() {
    dprintf("Y3 Dummy Cards: initialized\n");
    return S_OK;
}

HRESULT y3_io_close() {
    return S_OK;
}

HRESULT y3_io_get_cards(struct CardInfo* pCardInfo, int* numCards) {
    memset(pCardInfo, 0, sizeof(struct CardInfo) * *numCards);

    const float paddingX = 86.0f;
    const float paddingY = 54.0f;

    // Dimensions of the flat panel
    const float panelHeight = 1232.0f;
    const float panelWidth = 1160.0f;

    // Number of cards in each row and column
    const int numRows = 4;
    const int numCols = 4;
    int activeCards = numRows * numCols;

    if (*numCards < activeCards) {
        activeCards = *numCards;
    }
    *numCards = activeCards;


    // Calculate spacing between cards
    const float cardWidth = (panelWidth - paddingY * 2) / (numCols - 1);
    const float cardHeight = (panelHeight - paddingX * 2) / (numRows - 1);

    // Create example card info
    for (int i = 0; i < activeCards; i++) {
        int row = i / numCols;
        int col = i % numCols;

        pCardInfo[i].fX = paddingX + (col * cardHeight);
        pCardInfo[i].fY = paddingY + (row * cardWidth);
        pCardInfo[i].fAngle = 0.0f;
        pCardInfo[i].eCardType = TYPE0;
        pCardInfo[i].eCardStatus = VALID;
        pCardInfo[i].uID = 1000 + i;
        pCardInfo[i].nNumChars = 0;
        pCardInfo[i].ubChar0.Data = 0;
        pCardInfo[i].ubChar1.Data = 0;
        pCardInfo[i].ubChar2.Data = 0;
        pCardInfo[i].ubChar3.Data = 0;
        pCardInfo[i].ubChar4.Data = 0;
        pCardInfo[i].ubChar5.Data = 0;
    }

    return S_OK;
}