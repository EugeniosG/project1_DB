#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "block_records.h"
#include "hp_file.h"
#include "record.h"

/* Βοηθητικό macro: καλεί μια συνάρτηση του επιπέδου BF και, σε περίπτωση
   σφάλματος, εκτυπώνει την περιγραφή του και επιστρέφει HP_ERROR */
#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK) {      \
      BF_PrintError(code);    \
      return HP_ERROR;        \
    }                         \
  }

  static BF_Block *meta_block[BF_MAX_OPEN_FILES];
  static HP_info *meta_info[BF_MAX_OPEN_FILES];

HP_ErrorCode HP_CreateFile(const char *fileName) {
  /* TODO: δημιουργήστε το αρχείο, δεσμεύστε το πρώτο μπλοκ και αποθηκεύστε
     εκεί ένα στιγμιότυπο της HP_info */
  int fd ;  //file description
  BF_Block *block;
  CALL_BF(BF_CreateFile(fileName))
  CALL_BF(BF_OpenFile(fileName, &fd));
  BF_Block_Init(&block);
  CALL_BF(BF_AllocateBlock(fd, block));

  char *data = BF_Block_GetData(block);
  memset(data, 0, BF_BLOCK_SIZE);

  HP_info *info = (HP_info *) data;
  //strlen δίνει 8, και το + 1 προσθέτει το \0 ενω sizeof(info->type) είναι 10
  memcpy(info->type, "HEAPFILE", strlen("HEAPFILE") + 1);
  info->last_block_ID = 0;
  info->records_in_last_block = 0;
  info->max_records_per_block = RECORDS_PER_BLOCK;
  info->iter_block = 1;
  info->iter_slot = 0;

  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));
  BF_Block_Destroy(&block); 
  CALL_BF(BF_CloseFile(fd));

  return HP_OK;
}

HP_ErrorCode HP_OpenFile(const char *fileName, int *fileDesc) {

  /* TODO: ανοίξτε το αρχείο, διαβάστε τα μεταδεδομένα από το πρώτο μπλοκ και
    ελέγξτε ότι πρόκειται όντως για αρχείο σωρού */
  int fd;
  BF_Block *block;

  CALL_BF(BF_OpenFile(fileName, &fd));
  if(fd < 0 || fd >= BF_MAX_OPEN_FILES){
    BF_CloseFile(fd);
    return HP_ERROR;
  }
  BF_Block_Init(&block);
  
  if(BF_GetBlock(fd, 0, block) != BF_OK){
    BF_Block_Destroy(&block);
    CALL_BF(BF_CloseFile(fd));
    return HP_ERROR;
  }

  HP_info *info = (HP_info *) BF_Block_GetData(block);
  //strlen δίνει 8, και το + 1 προσθέτει το \0 ενω sizeof(info->type) είναι 10
  if (memcmp(info->type, "HEAPFILE", strlen("HEAPFILE") + 1) != 0){
    BF_UnpinBlock(block);
    BF_Block_Destroy(&block);
    CALL_BF(BF_CloseFile(fd));
    return HP_ERROR;
  }

  info->iter_block = 1;
  info->iter_slot = 0;

  meta_block[fd] = block;
  meta_info[fd] = info; 
  *fileDesc = fd;
  return HP_OK;
}

HP_ErrorCode HP_CloseFile(int fileDesc) {
  /* TODO: γράψτε στον δίσκο ό,τι εκκρεμεί και κλείστε το αρχείο */
if (fileDesc < 0 || fileDesc >= BF_MAX_OPEN_FILES)  //εκτός ορίων πίνακα
  return HP_ERROR;
if (meta_block[fileDesc] == NULL)                   //δεν είναι ανοιχτό
  return HP_ERROR;

CALL_BF(BF_UnpinBlock(meta_block[fileDesc]));       //(BF_Block *) unpin του block 0
BF_Block_Destroy(&meta_block[fileDesc]);            //meta_block[fileDesc] = NULL
meta_info[fileDesc] = NULL;                         // δεν ισχύει ο δείκτης στα δεδομένα πλέον

CALL_BF(BF_CloseFile(fileDesc));
  return HP_OK;
}

HP_ErrorCode HP_InsertEntry(int fileDesc, Record record) {
  /* TODO: βρείτε ή δεσμεύστε το κατάλληλο μπλοκ και αντιγράψτε εκεί την
     εγγραφή με την blockSetRecord. Μην ξεχάσετε να ενημερώσετε τα
     μεταδεδομένα του αρχείου. */
     //διαφάνειες σελ 18-20
if (fileDesc < 0 || fileDesc >= BF_MAX_OPEN_FILES)  //εκτός ορίων πίνακα
  return HP_ERROR;
if (meta_block[fileDesc] == NULL)                   //δεν είναι ανοιχτό
  return HP_ERROR;

HP_info *info = meta_info[fileDesc];
BF_Block *block;
BF_Block_Init(&block);

if (info->last_block_ID == 0 ||                    // δεν υπάρχει block 
    info->records_in_last_block == 6) {            // είναι γεμάτο
  CALL_BF(BF_AllocateBlock(fileDesc, block));     // νέο block στο τέλος
  info->last_block_ID++;
  info->records_in_last_block = 0;
} else {
  CALL_BF(BF_GetBlock(fileDesc, info->last_block_ID, block));
}

blockSetRecord(block, info->records_in_last_block, record); //record στην κενή θέση
info->records_in_last_block++;                              // +1 record
BF_Block_SetDirty(meta_block[fileDesc]);

CALL_BF(BF_UnpinBlock(block));
BF_Block_Destroy(&block);
  return HP_OK;
}

HP_ErrorCode HP_PrintAllEntries(int fileDesc, char *fieldName, void *value) {
  /* TODO: διατρέξτε τα μπλοκ του αρχείου και εκτυπώστε τις εγγραφές που στο
     πεδίο fieldName έχουν τιμή value. Αν value == NULL, εκτυπώστε τις όλες. */
  return HP_ERROR;
}

HP_ErrorCode HP_GetEntry(int fileDesc, int blockid, int position,
                         Record *record) {
  /* TODO: φέρτε το μπλοκ blockid και διαβάστε την εγγραφή της θέσης position
     με την blockGetRecord */
  return HP_ERROR;
}

HP_ErrorCode HP_GetNextRecordWithID(int fileDesc, Record *record, void *value) {
  /* TODO: υλοποιήστε τον iterator. Η κατάσταση του (πού σταμάτησε η
     προηγούμενη κλήση) πρέπει να επιβιώνει ανάμεσα στις κλήσεις. */
  return HP_ERROR;
}
