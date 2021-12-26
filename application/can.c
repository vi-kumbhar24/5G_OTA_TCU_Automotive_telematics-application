BYTE CheckCanTxList(DWORD id, CAN_PERIODIC_TX_MSG_STRUCT *messageList)
{
    BYTE bIndex;

    for (bIndex = 0; bIndex < CAN_ID_LIST_SIZE; bIndex++) {
        if (messageList->txMessage.id.l == id) {
            return bIndex;
        }
        messageList++;
    }

    return 0xFF;
}

BYTE AddMessageCanTxList(CAN_PERIODIC_TX_MSG_STRUCT *message, CAN_PERIODIC_TX_MSG_STRUCT *messageList)
{
    BYTE bIndex;

    //If this ID is already in the list, no need to add it
    bIndex = CheckCanTxList(message->txMessage.id.l, messageList);
    if (bIndex != 0xFF) {
        messageList += bIndex;    //This ID is already in the list.  Point to it
    } else {
        //This ID is not already in the list.  See if there's an open slot
        for (bIndex = 0; bIndex < CAN_ID_LIST_SIZE; bIndex++) {
            if (messageList->txMessage.id.l == 0xffffffff) {   //If this slot is available
                break;
            }
            messageList++;
        }
    }

    if (bIndex < CAN_ID_LIST_SIZE) {
      //If we found a slot
      memcpy((void *)messageList, (void *)message, sizeof(CAN_PERIODIC_TX_MSG_STRUCT));
      messageList->wRunTime = messageList->wPeriod; //Schedule the message;
      messageList->fTxEnable = true;  //Enable the message
      return bIndex;
    }

    return 0xFF;
}

BYTE DeleteMessageCanTxList(CAN_PERIODIC_TX_MSG_STRUCT *message, CAN_PERIODIC_TX_MSG_STRUCT *messageList)
{
    BYTE bIndex;

    for (bIndex = 0; bIndex < CAN_ID_LIST_SIZE; bIndex++) {
        if (messageList->txMessage.id.l == message->txMessage.id.l) {
            messageList->txMessage.id.l = 0xffffffff;
            messageList->wPeriod = 0;
            messageList->fTxEnable = false;
            return bIndex;
        }
        messageList++;
    }

    return 0xFF;
}

void ClearCanTxList(CAN_PERIODIC_TX_MSG_STRUCT *messageList)
{
    BYTE bIndex;

    for (bIndex = 0; bIndex < CAN_ID_LIST_SIZE; bIndex++) {
        messageList->txMessage.id.l = 0xffffffff;
        messageList->wPeriod = 0;
        messageList->fTxEnable = false;
        messageList++;
    }
}

BYTE RequestCanTx(CAN_PERIODIC_TX_MSG_STRUCT *messageList, BYTE bIndex)
{
    if (bIndex > CAN_ID_LIST_SIZE - 1) {
        return false;
    }
    messageList += bIndex;

    if (messageList->txMessage.id.l == 0xffffffff) {
        return false;
    }

    messageList->fTxRequest = true;

    return true;
}

void EnableCanTx(CAN_PERIODIC_TX_MSG_STRUCT *messageList, BYTE bIndex)
{
    if (bIndex < CAN_ID_LIST_SIZE) {
        messageList += bIndex;
        messageList->fTxEnable = true;  //Enable the message
        if (messageList->wPeriod != 0)
            messageList->wRunTime = messageList->wPeriod; //Schedule the message;
    }

    return;
}

void DisableCanTx(CAN_PERIODIC_TX_MSG_STRUCT *messageList, BYTE bIndex)
{
    if (bIndex < CAN_ID_LIST_SIZE) {
        messageList += bIndex;
        messageList->fTxEnable = false;  //Disable the message
    }

    return;
}

