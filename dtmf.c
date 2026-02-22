/* VUURWERK v1.0.4 - DTMF Stub Implementation
 * All DTMF functionality removed - these are just stubs
 */

#include "dtmf.h"
#include <string.h>

// Stub global variables
char gDTMF_String[15] = "---------------";
char gDTMF_InputBox[15] = "";
uint8_t gDTMF_InputBox_Index = 0;
bool gDTMF_InputMode = false;
char gDTMF_RX_live[20] = "";
uint8_t gDTMF_RX_live_timeout = 0;
// DTMF_RX_live_timeout_500ms is defined in misc.c, not here
char gDTMF_Caller[4] = "";
bool gIsDtmfContactValid = false;
uint8_t gDTMF_WriteIndex = 0;
uint8_t gDTMF_PreviousIndex = 0;
uint8_t gDTMF_ReplyState = 0;

// Stub function implementations
void DTMF_clear_input_box(void) {
	gDTMF_InputBox_Index = 0;
	gDTMF_InputMode = false;
}

void DTMF_Append(char c) {
	(void)c;
}

void DTMF_HandleRequest(void) {
}

void DTMF_Reply(void) {
}

char DTMF_GetCharacter(uint8_t code) {
	(void)code;
	return 0xFF;
}

bool DTMF_GetContact(const char *pContact, char *pName) {
	(void)pContact;
	(void)pName;
	return false;
}

bool DTMF_FindContact(const char *pContact, char *pResult) {
	(void)pContact;
	(void)pResult;
	return false;
}

void DTMF_SendEndOfTransmission(void) {
}

bool DTMF_ValidateCodes(char *pCode, uint8_t size) {
	(void)pCode;
	(void)size;
	return true; // Stub: always return valid
}
