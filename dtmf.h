/* VUURWERK v1.0.4 - DTMF Stub (feature removed)
 * This file provides empty declarations to allow compilation
 * All DTMF functionality has been removed to save flash space
 */

#ifndef DTMF_H
#define DTMF_H

#include <stdint.h>
#include <stdbool.h>

// Stub global variables
extern char gDTMF_String[15];
extern char gDTMF_InputBox[15];
extern uint8_t gDTMF_InputBox_Index;
extern bool gDTMF_InputMode;
extern char gDTMF_RX_live[20];
extern uint8_t gDTMF_RX_live_timeout;
extern const uint8_t DTMF_RX_live_timeout_500ms;
extern char gDTMF_Caller[4];
extern bool gIsDtmfContactValid;
extern uint8_t gDTMF_WriteIndex;
extern uint8_t gDTMF_PreviousIndex;
extern uint8_t gDTMF_ReplyState;

// DTMF reply states
#define DTMF_REPLY_NONE 0
#define DTMF_REPLY_ANI  1
#define DTMF_REPLY_AB   2

// Stub function declarations
void DTMF_clear_input_box(void);
void DTMF_Append(char c);
void DTMF_HandleRequest(void);
void DTMF_Reply(void);
char DTMF_GetCharacter(uint8_t code);
bool DTMF_GetContact(const char *pContact, char *pName);
bool DTMF_FindContact(const char *pContact, char *pResult);
void DTMF_SendEndOfTransmission(void);
bool DTMF_ValidateCodes(char *pCode, uint8_t size);

#endif // DTMF_H
