// -----------------------------------------------------------------------------
// -----                                                                   -----
// -----                           R3BCaloUnpack                           -----
// -----                           Version 1.0                             -----
// -----                    Created  11/10/2013 by Y.Gonzalez              -----
// -----                    Modified 03/03/2014 by M. Bendel               -----
// -----                    Modified 28/09/2014 by A.Perea                 -----                
// -----                                                                   -----
// -----------------------------------------------------------------------------

//ROOT headers
#include "TClonesArray.h"

//Fair headers
#include "FairRootManager.h"
#include "FairRunAna.h"
#include "FairRuntimeDb.h"
#include "FairLogger.h"

#include <iomanip>

//Califa headers
#include "R3BCaloRawHit.h"
#include "R3BCaloUnpack.h"



//------------------------
/**
 * Constructor
 */
R3BCaloUnpack::R3BCaloUnpack(char *strCalDir,
                             Short_t type, Short_t subType,
                             Short_t procId,
                             Short_t subCrate, Short_t control)
  : FairUnpack(type, subType, procId, subCrate, control),
    fRawData(new TClonesArray("R3BCaloRawHit")),
    fNHits(0),fCaloUnpackPar(0)
{
}





//-------------------------
/**
 * Destructor
 */
R3BCaloUnpack::~R3BCaloUnpack()
{
  LOG(INFO) << "R3BCaloUnpack: Delete instance" << FairLogger::endl;
  delete fRawData;
}




//--------------------------
/**
 *  
 */
Bool_t R3BCaloUnpack::Init() {
  Register();
  return kTRUE;
}



//--------------------------
/**
 * 
 */
void R3BCaloUnpack::SetParContainers() {
  
  // Get run and runtime database
  FairRunAna* run = FairRunAna::Instance();
  if (!run) Fatal("R3BCaloUnpack::SetParContainers", "No analysis run");
  
  FairRuntimeDb* rtdb = run->GetRuntimeDb();
  if (!rtdb) Fatal("R3BCaloUnpack::SetParContainers", "No runtime database");
  
  fCaloUnpackPar = (R3BCaloUnpackPar*)(rtdb->getContainer("R3BCaloUnpackPar"));
  
  if ( fCaloUnpackPar ) {
    LOG(INFO) << "R3BCaloUnpack::SetParContainers() "<< FairLogger::endl;
    LOG(INFO) << "Container R3BCaloUnpackPar loaded " << FairLogger::endl;
  }  
}


//----------------------------
/*
 * 
 */
void R3BCaloUnpack::Register() {
  LOG(DEBUG) << "Registering" << FairLogger::endl;
  FairRootManager *fMan = FairRootManager::Instance();
  if(! fMan) {
    return;
  }
  fMan->Register("CaloRawHit", "Raw data from Califa", fRawData, kTRUE);
}



//------------------------------
/*  R3BCalounpack
 * 
 * Called by the unpacker, returns
 * 
 */
Bool_t R3BCaloUnpack::DoUnpack(Int_t *data, Int_t size) {
  
  UInt_t *pl_data = (UInt_t*) data;
  UInt_t l_s = 0; // skip over timerabit header
  UInt_t start;      // used to decect optional payloads
  
  //----- Whiterabbit timestamp ------
  // The structure of the time-stamp is as follows
  // 0x03E1 XXXX
  // 0x04E1 YYYY
  // 0x05E1 ZZZZ
  // 0x06E1 TTTT
  // Where the whiteRabbit time-stamp is a 64bit-long integer = 0xTTTTZZZZYYYYXXXX
    
  ULong64_t rabbit0, rabbit1, rabbit2, rabbit3, rabbit4;
  ULong64_t check0, check1, check2, check3;
 
  l_s++; // skip first module id (should be 0x0300)
  check0 = ( data[l_s] >> 16 ) & 0xffff;  rabbit1 =  data[l_s++] & 0xffff;
  check1 = ( data[l_s] >> 16 ) & 0xffff;  rabbit2 =  data[l_s++] & 0xffff;
  check1 = ( data[l_s] >> 16 ) & 0xffff;  rabbit3 =  data[l_s++] & 0xffff;
  check1 = ( data[l_s] >> 16 ) & 0xffff;  rabbit4 =  data[l_s++] & 0xffff;
  
  ULong64_t rabbitStamp = (rabbit4 << 48) | (rabbit3 << 32) | ( rabbit2 << 16 ) | rabbit1; 
 /* LOG(DEBUG) << "-------- EVENT ----------" << FairLogger::endl;
  LOG(DEBUG) << "whiteRabbit:" << rabbitStamp << FairLogger::endl;
 */ 
  //---------- hit data ---------
  
  // hitdata consists in several hits 
  // there are two possible formats (old and new), a magic number defines version is being used. 
  // In the new format, each hit can be followed by optional extra data for time-over-threshold or trace
  
  LOG(DEBUG) << "Unpacking" << FairLogger::endl;

  while(l_s < size) {

    // @TODO Loops over CALIFA halves to be implemented
    
    //----------- parse header -----------
    
    // Remove 0xadd... words. some padding?
    while((data[l_s] & 0xfff00000) == 0xadd00000) { l_s++;  }
    
    UShort_t header_size;
    UShort_t trigger;
    UShort_t pc_id = 0;     //Should be implemented for use of two CALIFA halves
    UShort_t sfp_id = 0;
    UShort_t card;
    UShort_t channel;
    UInt_t   data_size;

    header_size = pl_data[l_s] & 0xff;
    trigger = (pl_data[l_s] >> 8) & 0xff;
    card = (pl_data[l_s] >> 16) & 0xff;
    channel = (pl_data[l_s++] >> 24) & 0xff;
    data_size = pl_data[l_s++];
    
    
//     LOG(INFO) << "========== gosip sub " << FairLogger::endl
//      << "     header_size " << header_size << FairLogger::endl
//      << "         trigger " << trigger << FairLogger::endl
//      << "            card " << card << FairLogger::endl
//      << "         channel " << channel << FairLogger::endl
//      << "       data_size " << data_size << FairLogger::endl;

    
    if(header_size != 0x34) {
      LOG(WARNING) << "Wrong header size ( is " << header_size << ")" << FairLogger::endl;
      break;
    }
    
    // Data reduction: size == 0 -> no more events
    if(data_size == 0) continue;
  
    
    // ignore special channel with metadata, if present
    if(channel == 0xff) {
      //!! Prepared for use with different SFPs    !!
      //!! not available in current data structure !!
      // l_s++;
      // sfp_id = (pl_data[l_s++] >> 24) & 0xff;
      l_s += data_size / 4;
      continue;
    }
 
 
  
    //----------- parse data -----------
             
    UShort_t evsize;
    UShort_t magic;            // magic number to tell appart data format versions
    UInt_t event_id;
    ULong_t febexTimestamp;    // timestamp from the febex. Not used
    UShort_t cfd_samples[4]; 
    UShort_t loverflow;
    UShort_t hoverflow;
    UInt_t   overflow;
    UShort_t self_triggered;
    UShort_t num_pileup;
    UShort_t num_discarded;
    UShort_t energy;
    //    UShort_t reserved;
    UShort_t qpid_size;
    UShort_t magic_babe;
    Short_t n_f;
    UShort_t n_s;
    UChar_t error = 0;
    UShort_t tot;
    UShort_t tot_samples[4];
 
    start = l_s;                               // used to calc readed data vs evsize
    evsize = pl_data[l_s] & 0xffff;            // first word of the data 
    magic = (pl_data[l_s++] >> 16) & 0xffff;
   
    
    switch(magic) {      
      
      case 0xAFFE: // old version of lmd. 1 (evsize & magic) + 10 words, 44 bytes
        event_id = pl_data[l_s++];
        febexTimestamp = pl_data[l_s++];   // not used. see whiterabbit timestamp above
        febexTimestamp |= (ULong_t) pl_data[l_s++] << 32;
        cfd_samples[0] = pl_data[l_s] & 0xffff;
        cfd_samples[1] = pl_data[l_s++] >> 16;
        cfd_samples[2] = pl_data[l_s] & 0xffff;
        cfd_samples[3] = pl_data[l_s++] >> 16;
        overflow = pl_data[l_s] & 0xffffff;
        self_triggered = (pl_data[l_s++] >> 24) & 0xff;
        num_pileup = pl_data[l_s] & 0xffff;
        num_discarded = (pl_data[l_s++] >> 16) & 0xffff;
        energy = pl_data[l_s++] & 0xffff;
        //energy = (int32_t)((uint16_t)(pl_data[l_s++] & 0xffff));     // in Max's unpacker
        l_s++;
        n_f = pl_data[l_s] & 0xffff;
        n_s = (pl_data[l_s++] >> 16) & 0xffff;
        //n_f = (int32_t)((uint16_t)(pl_data[l_s] & 0xffff));           // in Max's unpacker
        //n_s = (int32_t)((uint16_t)((pl_data[l_s++] >> 16) & 0xffff)); // in Max's unpacker
        break;
        
      case 0x115A:  // new event version: 1 (evsize & magic) + 9 words = 40 bytes
        event_id = pl_data[l_s++];
        febexTimestamp = pl_data[l_s++];
        febexTimestamp |= (ULong_t)pl_data[l_s++] << 32; // not used. see whiterabbit timestamp above
        cfd_samples[0] = pl_data[l_s] & 0xff;
        cfd_samples[1] = pl_data[l_s++] >> 16;
        cfd_samples[2] = pl_data[l_s] & 0xff;
        cfd_samples[3] = pl_data[l_s++] >> 16;
        overflow = pl_data[l_s] & 0xffffff;
        self_triggered = (pl_data[l_s++] >> 24) & 0xff;
        num_pileup = pl_data[l_s] & 0xffff;
        num_discarded = (pl_data[l_s++] >> 16) & 0xffff;
        energy = pl_data[l_s++] & 0xffff;
        // field not included in new version
        n_f = pl_data[l_s] & 0xffff;
        n_s = (pl_data[l_s++] >> 16) & 0xffff;
        
        // checks if optional time-over-threshold payload present (recognized by 0xBEEF as first word) 
        if ( (evsize > 4 * (l_s - start))   && ( ((pl_data[l_s] >> 16) & 0xffff) == 0xBEEF) ) {
          LOG(DEBUG) << "TOT payload present" << FairLogger::endl;
          tot = pl_data[l_s++] & 0xffff;
          tot_samples[0] = pl_data[l_s] & 0xffff;
          tot_samples[1] = (pl_data[l_s++] >> 16) & 0xffff;
          tot_samples[2] = pl_data[l_s] & 0xffff;
          tot_samples[3] = (pl_data[l_s++] >> 16) & 0xffff; 
        }
          
//         if( (evsize > kEvent115a_t_size) && ((pl_data[l_s] >> 16) & 0xffff) == 0xBEEF)  { // 4 words
//           LOG(INFO) << "TOT payload present" << FairLogger::endl;
//           tot = pl_data[l_s++] & 0xffff;
//           tot_samples[0] = pl_data[l_s] & 0xffff;
//           tot_samples[1] = (pl_data[l_s++] >> 16) & 0xffff;
//           tot_samples[2] = pl_data[l_s] & 0xffff;
//           tot_samples[3] = (pl_data[l_s++] >> 16) & 0xffff;
//         }

        // checks if traces are present --------------
        //LOG(INFO) << evsize << " " << l_s - start << FairLogger::endl;
        
        if (evsize > 4 * (l_s - start) ) {  // there is still traces in the 
          //LOG(DEBUG) << "TRACE payload present" << FairLogger::endl;
          l_s = start + (evsize / 4); // skip the traces
        }
        
        break;
                                    
      default:
        LOG(WARNING) << "Invalid event magic number:" << magic << FairLogger::endl; 
        break;
            
      } // case
    


    // Set error flags
    // Error flag structure: [Pileup][PID][Energy][Timing]

    if (overflow & 0x601)  error |= 1; //11000000001 Timing not valid
    if (overflow & 0x63e)   error |= 1<<1; //11000111110  Energy not valid
    if (overflow & 0x78E)    error |= 1<<2; //11110001110  PID not valid
    if (num_pileup)   error |= 1<<3;
    

     // Generates crystalID out of  pc, crate, board, channel
     UShort_t crystal_id = pc_id * (max_channel*max_card*max_sfp_id)
                        + sfp_id * (max_channel*max_card)
                        + card * max_channel
                        + channel;

    new ((*fRawData)[fNHits]) R3BCaloRawHit(crystal_id, energy, n_f, n_s, rabbitStamp, error, tot);
    fNHits++;
  
  } // while
 
  return kTRUE;

} // DoUnpack


//--------------------
/**
 * 
 * 
 */
void R3BCaloUnpack::Reset() {
  LOG(DEBUG) << "Clearing Data Structure" << FairLogger::endl;
  fRawData->Clear();
  fNHits = 0;
}

//---------------------
ClassImp(R3BCaloUnpack)
