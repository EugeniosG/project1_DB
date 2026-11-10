#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

/* Τα αρχεία βάσης δημιουργούνται στον κατάλογο DBs/ του project */
#define DB_FILE "DBs/myheap.db"

/*
 * O driver της ασκησης.
 *
 * Το εκτελέσιμο πρέπει να διαβάζει τις εγγραφές που αποθηκεύει από ένα αρχείο
 * κειμένου, το οποίο υποδεικνύεται με τη σημαία -i:
 *
 *     ./bin/myheap -i inputs/records.txt
 *
 * Κάθε γραμμή του αρχείου περιέχει μία εγγραφή, με τα πεδία χωρισμένα με κενά
 * και στη σειρά που έχουν στη δομή Record:
 *
 *     id firstname lastname postcode city
 *     151 Dimitra Stasinou 12376 Ioannina
 *
 * Ο driver θα πρέπει να δημιουργεί το αρχείο σωρού, να εισάγει σε αυτό τις
 * εγγραφές του αρχείου εισόδου, και να επιδεικνύει τη λειτουργία των
 * συναρτήσεων του API.
 */

int main(int argc, char *argv[]) {
  if (argc != 3)      //argv[0] ./bin/myheap	argv[1]	-i  argv[2]	inputs/records.txt
    return EXIT_FAILURE;
  int fd;                          // file descriptor του heap file (από HP_OpenFile)
  Record r;
  remove(DB_FILE);
  FILE *in = fopen(argv[2], "r");  //ανοίγει το records.txt για read
  if(in == NULL){
    return EXIT_FAILURE;
  }
  BF_Init(LRU);                    // ξεκινάει τη BF με αντικατάσταση LRU

  if (HP_CreateFile(DB_FILE) != HP_OK)   // τρέχει πρώτα make clean 
    return EXIT_FAILURE;
  HP_OpenFile(DB_FILE, &fd);       //open heap file κ πάρε fd

  while (fscanf(in, "%d %24s %19s %d %19s",     //id firstname lastname postcode city
                &r.id, r.firstname, r.lastname, &r.postcode, r.city) == 5) //οσο βγάζει 5 υπάρχει εγγραφή
    HP_InsertEntry(fd, r);            // imsert στο heap file
  fclose(in);                 //κλείσιμο αρχείου

  printf("\nΌλες οι εγγραφές:\n");
  HP_PrintAllEntries(fd, NULL, NULL);                 // όλες οι 15
  HP_PrintAllEntries(fd, "city", "Athina");           // μονο city=Athina

  printf("\nHP_GetEntry (block 1, θέση 0):\n");
  HP_GetEntry(fd, 1, 0, &r);                          // block 1, θέση 0
  printRecord(r);
  
  printf("\nHP_GetEntry με θέση πού δεν υπάρχει\n");
  if (HP_GetEntry(fd, 1, 1000, &r) == HP_ERROR){    // η θέση 1000 δεν υπάρχει
    printf("HP_ERROR. As we expected\n");
  }

  printf("\nHP_OpenFile σε αρχείο που δεν είναι heap\n");
  if (HP_OpenFile("DBs/bf.db", &fd) == HP_ERROR){
    printf("HP_ERROR. As we expected\n");
  }

  int id = 151;
  printf("\nHP_GetNextRecordWithID (id = 151):\n");
  while (HP_GetNextRecordWithID(fd, &r, &id) == HP_OK)  // iterator μέχρι HP_EOF
    printRecord(r);

  HP_CloseFile(fd);               // unpin block 0
  BF_Close();                     //κλείνει την BF γραφει στο δίσκο οιτ εκρεμμεί
  return EXIT_SUCCESS;            
}
