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
/* κατάσταση ανά ανοιχτό αρχείο, με δείκτη το fd (χωρίς malloc) */
static BF_Block *meta_block[BF_MAX_OPEN_FILES];     // pinned block 0
static HP_info *meta_info[BF_MAX_OPEN_FILES];       // HP_info μέσα στο block 0

HP_ErrorCode HP_CreateFile(const char *fileName) {
  /* DONE: δημιουργήστε το αρχείο, δεσμεύστε το πρώτο μπλοκ και αποθηκεύστε
     εκεί ένα στιγμιότυπο της HP_info */
  int fd ;  //file description
  BF_Block *block;
  CALL_BF(BF_CreateFile(fileName))
  CALL_BF(BF_OpenFile(fileName, &fd));
  BF_Block_Init(&block);
  CALL_BF(BF_AllocateBlock(fd, block));

  char *data = BF_Block_GetData(block);
  memset(data, 0, BF_BLOCK_SIZE);

  HP_info *info = (HP_info *) data;     // το HP_info στην αρχή του block 0
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

  /* DONE: ανοίξτε το αρχείο, διαβάστε τα μεταδεδομένα από το πρώτο μπλοκ και
    ελέγξτε ότι πρόκειται όντως για αρχείο σωρού */
  //ξεκινάει από (1, 0), πρώτη εγγραφή
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

  info->iter_block = 1;         // iterator από την αρχή (1, 0)
  info->iter_slot = 0;

  meta_block[fd] = block;     // μένει pinned μέχρι HP_CloseFile
  meta_info[fd] = info; 
  *fileDesc = fd;
  return HP_OK;
}

HP_ErrorCode HP_CloseFile(int fileDesc) {
  /* DONE: γράψτε στον δίσκο ό,τι εκκρεμεί και κλείστε το αρχείο */
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
  /* DONE: βρείτε ή δεσμεύστε το κατάλληλο μπλοκ και αντιγράψτε εκεί την
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
    info->records_in_last_block == info->max_records_per_block) {     // είναι γεμάτο
  CALL_BF(BF_AllocateBlock(fileDesc, block));     // νέο block στο τέλος
  info->last_block_ID++;
  info->records_in_last_block = 0;
} else {
  CALL_BF(BF_GetBlock(fileDesc, info->last_block_ID, block));
}

blockSetRecord(block, info->records_in_last_block, record); //record στην κενή θέση
info->records_in_last_block++;                              // +1 record
BF_Block_SetDirty(meta_block[fileDesc]);              //metadata (count, last) → dirty

CALL_BF(BF_UnpinBlock(block));
BF_Block_Destroy(&block);
  return HP_OK;
}

HP_ErrorCode HP_PrintAllEntries(int fileDesc, char *fieldName, void *value) {
  /* DONE: διατρέξτε τα μπλοκ του αρχείου και εκτυπώστε τις εγγραφές που στο
     πεδίο fieldName έχουν τιμή value. Αν value == NULL, εκτυπώστε τις όλες. */

  if (fileDesc < 0 || fileDesc >= BF_MAX_OPEN_FILES)  // εκτός ορίων πίνακα
    return HP_ERROR;
  if (meta_block[fileDesc] == NULL)                   // δεν είναι ανοιχτό
    return HP_ERROR;

  HP_info *info = meta_info[fileDesc];
  BF_Block *block;
  BF_Block_Init(&block);

  for (int b = 1; b <= info->last_block_ID; b++) {                   // όλα τα block
    CALL_BF(BF_GetBlock(fileDesc, b, block));

    int n;                                            // πόσες εγγραφές έχει το block
    if (b == info->last_block_ID)
      n = info->records_in_last_block;               //το τελευταίο
    else
      n = info->max_records_per_block;               //τα άλλα είναι γεμάτα 

    for (int slot = 0; slot < n; slot++) {        //scan ολα τα slots
      Record rec;
      blockGetRecord(block, slot, &rec);

      int match = 0;        //έλεγχος αν ταιριάζει η εγγραφή, 1 = match, 0 = no match
      if (value == NULL)
        match = 1;        
      else if (strcmp(fieldName, "id") == 0)
        match = (rec.id == *(int *)value);            //int values:
      else if (strcmp(fieldName, "postcode") == 0)
        match = (rec.postcode == *(int *)value);
      else if (strcmp(fieldName, "firstname") == 0)
        match = (strcmp(rec.firstname, (char *)value) == 0);    //char  values:
      else if (strcmp(fieldName, "lastname") == 0)
        match = (strcmp(rec.lastname, (char *)value) == 0);
      else if (strcmp(fieldName, "city") == 0)
        match = (strcmp(rec.city, (char *)value) == 0);

      if (match)
        printRecord(rec);
    }
    CALL_BF(BF_UnpinBlock(block));
  }

  BF_Block_Destroy(&block);
  return HP_OK;
}

HP_ErrorCode HP_GetEntry(int fileDesc, int blockid, int position,
                         Record *record) {
  /* DONE: φέρτε το μπλοκ blockid και διαβάστε την εγγραφή της θέσης position
     με την blockGetRecord */
  if (fileDesc < 0 || fileDesc >= BF_MAX_OPEN_FILES)    // εκτός ορίων
    return HP_ERROR;
  if (meta_block[fileDesc] == NULL)                 // δεν είναι ανοιχτό
    return HP_ERROR;

  HP_info *info = meta_info[fileDesc];

  if (blockid < 1 || blockid > info->last_block_ID)       // δεν υπάρχει τέτοιο block
    return HP_ERROR;

  int n;                                              //πόσες εγγραφές στο block
  if (blockid == info->last_block_ID)
    n = info->records_in_last_block;          // τελευταίο
  else
    n = info->max_records_per_block;          //τα άλλα είναι γεμάτα

  if (position < 0 || position >= n)       // κενo slot
    return HP_ERROR;

  BF_Block *block;
  BF_Block_Init(&block);
  CALL_BF(BF_GetBlock(fileDesc, blockid, block));     // φέρε το block (pin)
  blockGetRecord(block, position, record);            // αντιγραφή εγγραφής στο *record
  CALL_BF(BF_UnpinBlock(block));                      // unpin
  BF_Block_Destroy(&block);
  return HP_OK;
}

HP_ErrorCode HP_GetNextRecordWithID(int fileDesc, Record *record, void *value) {
  /* DONE: υλοποιήστε τον iterator. Η κατάσταση του (πού σταμάτησε η
     προηγούμενη κλήση) πρέπει να επιβιώνει ανάμεσα στις κλήσεις. */
  if (fileDesc < 0 || fileDesc >= BF_MAX_OPEN_FILES)   // εκτός ορίων πίνακα
    return HP_ERROR;
  if (meta_block[fileDesc] == NULL)                   // δεν είναι ανοιχτό
    return HP_ERROR;
  if (value == NULL)                                  // id για αναζήτηση
    return HP_ERROR;

  HP_info *info = meta_info[fileDesc];
  BF_Block *block;
  BF_Block_Init(&block);

  while (info->iter_block <= info->last_block_ID) {   // από το block που σταματησε
    CALL_BF(BF_GetBlock(fileDesc, info->iter_block, block));

    int n;                                            // πόσες εγγραφές στο block
    if (info->iter_block == info->last_block_ID)
      n = info->records_in_last_block;          // τελευταίο
    else
      n = info->max_records_per_block;          //τα άλλα είναι γεμάτα

    while (info->iter_slot < n) {                     // από τη θέση που σταμάτησε
      Record rec;
      blockGetRecord(block, info->iter_slot, &rec);
      info->iter_slot++;                  //ο iterator προχωρά στο επόμενο slot
      if (rec.id == *(int *)value) {            // σύγκριση id με την τιμή που δείχνει το value(: void* → int*)
        *record = rec;
        CALL_BF(BF_UnpinBlock(block));        //found: unnpin first
        BF_Block_Destroy(&block);
        return HP_OK;
      }
    }

    CALL_BF(BF_UnpinBlock(block));
    info->iter_block++;                               // επόμενο block
    info->iter_slot = 0;                              // από την αρχή
  }

  info->iter_block = 1;                            //τέλος iterator
  info->iter_slot  = 0;                            // από την αρχή
  BF_Block_Destroy(&block);
  return HP_EOF;
}
