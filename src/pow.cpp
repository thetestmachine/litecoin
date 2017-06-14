// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "primitives/block.h"
#include "uint256.h"
#include "util.h"
#include <inttypes.h>


static const int64_t nDiffChangeTarget = 56000; // Patch effective @ block 56000

static const int64_t nTargetTimespan = 3.5 * 24 * 60 * 60; // Einsteinium: 3.5 days
static const int64_t nTargetTimespanNEW = 60;
static const int64_t nTargetSpacing = 60; // Einsteinium: one minute
static const int64_t nInterval = nTargetTimespan / nTargetSpacing;


unsigned int DigiShield(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
	const arith_uint256 bnProofOfWorkLimit = UintToArith256(params.powLimit);
	
    int64_t retargetTimespan = nTargetTimespanNEW;
    int64_t retargetInterval = nTargetTimespanNEW / nTargetSpacing;

    // Only change once per interval
    if ((pindexLast->nHeight+1) % retargetInterval != 0)
    {
        return pindexLast->nBits;
    }

    // Einsteinium: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = retargetInterval-1;
    if ((pindexLast->nHeight+1) != retargetInterval)
        blockstogoback = retargetInterval;

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
    printf(" nActualTimespan = %" PRId64 " before bounds\n", nActualTimespan);
    
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    
 //DigiShield implementation - thanks to RealSolid & WDC for this code
// amplitude filter - thanks to daft27 for this code
        nActualTimespan = retargetTimespan + (nActualTimespan - retargetTimespan)/8;
        printf("DIGISHIELD RETARGET\n");
        if (nActualTimespan < (retargetTimespan - (retargetTimespan/4)) ) nActualTimespan = (retargetTimespan - (retargetTimespan/4));
        if (nActualTimespan > (retargetTimespan + (retargetTimespan/2)) ) nActualTimespan = (retargetTimespan + (retargetTimespan/2));
    // Retarget
    
    bnNew *= nActualTimespan;
    bnNew /= retargetTimespan;

    if (bnNew > bnProofOfWorkLimit)
        bnNew = bnProofOfWorkLimit;

    /// debug print
    printf("GetNextWorkRequired: DIGISHIELD RETARGET\n");
    printf("nTargetTimespan = %" PRId64 " nActualTimespan = %" PRId64 "\n", retargetTimespan, nActualTimespan);
    printf("Before: %08x %s\n", pindexLast->nBits, ArithToUint256(arith_uint256().SetCompact(pindexLast->nBits)).ToString().c_str());
    printf("After: %08x %s\n", bnNew.GetCompact(), ArithToUint256(bnNew).ToString().c_str());

    return bnNew.GetCompact();
}


unsigned int KimotoGravityWell(const CBlockIndex* pindexLast, const CBlockHeader *pblock, uint64_t TargetBlocksSpacingSeconds, uint64_t PastBlocksMin, uint64_t PastBlocksMax, const Consensus::Params& params) {
        /* current difficulty formula - kimoto gravity well */
        const CBlockIndex *BlockLastSolved                                = pindexLast;
        const CBlockIndex *BlockReading                                = pindexLast;
        const CBlockHeader *BlockCreating                                = pblock;
                                                BlockCreating                                = BlockCreating;
        uint64_t                                PastBlocksMass                                = 0;
        int64_t                                PastRateActualSeconds                = 0;
        int64_t                                PastRateTargetSeconds                = 0;
        double                                PastRateAdjustmentRatio                = double(1);
        arith_uint256                                PastDifficultyAverage;
        arith_uint256                                PastDifficultyAveragePrev;
        double                                EventHorizonDeviation;
        double                                EventHorizonDeviationFast;
        double                                EventHorizonDeviationSlow;
		
		const arith_uint256 bnProofOfWorkLimit = UintToArith256(params.powLimit);
        
    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || (uint64_t)BlockLastSolved->nHeight < PastBlocksMin) { return bnProofOfWorkLimit.GetCompact(); }
        
        for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
                if (PastBlocksMax > 0 && i > PastBlocksMax) { break; }
                PastBlocksMass++;
                
                if (i == 1) { PastDifficultyAverage.SetCompact(BlockReading->nBits); }
                else {
					// Quick fix for avoiding negative numbers (arith_uint256 is unsigned int256 so it can't work with negative numbers)
					arith_uint256 nBitsCompact = arith_uint256().SetCompact(BlockReading->nBits);
					if (nBitsCompact > PastDifficultyAveragePrev)
						PastDifficultyAverage = PastDifficultyAveragePrev + ((nBitsCompact - PastDifficultyAveragePrev) / i);
					else
						PastDifficultyAverage = PastDifficultyAveragePrev - ((PastDifficultyAveragePrev - nBitsCompact) / i);					
				}
                PastDifficultyAveragePrev = PastDifficultyAverage;
                
                PastRateActualSeconds                        = BlockLastSolved->GetBlockTime() - BlockReading->GetBlockTime();
                PastRateTargetSeconds                        = TargetBlocksSpacingSeconds * PastBlocksMass;
                PastRateAdjustmentRatio                        = double(1);
                if (PastRateActualSeconds < 0) { PastRateActualSeconds = 0; }
                if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
                PastRateAdjustmentRatio                        = double(PastRateTargetSeconds) / double(PastRateActualSeconds);
                }
                EventHorizonDeviation                        = 1 + (0.7084 * pow((double(PastBlocksMass)/double(144)), -1.228));
                EventHorizonDeviationFast                = EventHorizonDeviation;
                EventHorizonDeviationSlow                = 1 / EventHorizonDeviation;
                
                if (PastBlocksMass >= PastBlocksMin) {
                        if ((PastRateAdjustmentRatio <= EventHorizonDeviationSlow) || (PastRateAdjustmentRatio >= EventHorizonDeviationFast)) { assert(BlockReading); break; }
                }
                if (BlockReading->pprev == NULL) { assert(BlockReading); break; }
                BlockReading = BlockReading->pprev;
        }
        
        arith_uint256 bnNew(PastDifficultyAverage);
        if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
                bnNew *= PastRateActualSeconds;
                bnNew /= PastRateTargetSeconds;
        }
    if (bnNew > bnProofOfWorkLimit) { bnNew = bnProofOfWorkLimit; }
        
/* debug print (commented out due to spamming logs when the loop above breaks)
   printf("Difficulty Retarget - Kimoto Gravity Well\n");
   printf("PastRateAdjustmentRatio = %g\n", PastRateAdjustmentRatio);
   printf("Before: %08x %s\n", BlockLastSolved->nBits, arith_uint256().SetCompact(BlockLastSolved->nBits).getuint256().ToString().c_str());
   printf("After: %08x %s\n", bnNew.GetCompact(), bnNew.getuint256().ToString().c_str());
*/ 
      
        return bnNew.GetCompact();
}


unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{

	int nHeight = pindexLast->nHeight + 1;
	bool fNewDifficultyProtocol = (nHeight >= nDiffChangeTarget);

	if (fNewDifficultyProtocol) {
		return DigiShield(pindexLast, pblock, params);
	}
	else {

        static const int64_t	      	 BlocksTargetSpacing 			 = 60; // 1 minute
        unsigned int                       TimeDaySeconds                                = 60 * 60 * 24;
        int64_t                                PastSecondsMin                                = TimeDaySeconds * 0.25;
        int64_t                                PastSecondsMax                                = TimeDaySeconds * 7;
        uint64_t                                PastBlocksMin                                = PastSecondsMin / BlocksTargetSpacing;
        uint64_t                                PastBlocksMax                                = PastSecondsMax / BlocksTargetSpacing;

        return KimotoGravityWell(pindexLast, pblock, BlocksTargetSpacing, PastBlocksMin, PastBlocksMax, params);
	}
}



unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    arith_uint256 bnNew;
    arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    // Einsteinium: intermediate uint256 can overflow by 1 bit
    bool fShift = bnNew.bits() > 235;
    if (fShift)
        bnNew >>= 1;
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;
    if (fShift)
        bnNew <<= 1;

    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
