/* 
   Copyright (C) 2013 Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"


void
test_compareandwrite_simple(void)
{
	int i, ret;
	unsigned j;
	unsigned char *buf = alloca(2 * 256 * block_size);
	int maxbl;

	CHECK_FOR_DATALOSS;
	CHECK_FOR_SBC;

	if (inq_bl && inq_bl->max_cmp) {
		maxbl = inq_bl->max_cmp;
	} else {
		/* Assume we are not limited */
		maxbl = 256;
	}

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test COMPARE_AND_WRITE of 1-256 blocks at the "
		"start of the LUN");
	for (i = 1; i < 256; i++) {
		logging(LOG_VERBOSE, "Write %d blocks of 'A' at LBA:0", i);
		memset(buf, 'A', 2 * i * block_size);
		if (maximum_transfer_length && maximum_transfer_length < i) {
			break;
		}
		ret = write16(sd, 0, i * block_size,
			      block_size, 0, 0, 0, 0, 0, buf,
			      EXPECT_STATUS_GOOD);
		if (ret == -2) {
			logging(LOG_NORMAL, "[SKIPPED] WRITE16 is not implemented.");
			CU_PASS("WRITE16 is not implemented.");
			return;
		}	
		CU_ASSERT_EQUAL(ret, 0);

		if (i > maxbl) {
			logging(LOG_VERBOSE, "Number of blocks %d is greater than "
				"BlockLimits.MaximumCompareAndWriteLength(%d). "
				"Command should fail with INVALID_FIELD_IN_CDB",
				i, maxbl);
			ret = compareandwrite(sd, 0,
					      buf, 2 * i * block_size,
					      block_size, 0, 0, 0, 0,
					      EXPECT_INVALID_FIELD_IN_CDB);
			if (ret == -2) {
				CU_PASS("[SKIPPED] Target does not support "
					"COMPARE_AND_WRITE. Skipping test");
				return;
			}
			CU_ASSERT_EQUAL(ret, 0);

			continue;
		}

		memset(buf + i * block_size, 'B', i * block_size);

		logging(LOG_VERBOSE, "Overwrite %d blocks with 'B' "
			"at LBA:0 (if they all contain 'A')", i);
		ret = compareandwrite(sd, 0,
				      buf, 2 * i * block_size, block_size,
				      0, 0, 0, 0,
				      EXPECT_STATUS_GOOD);
		if (ret == -2) {
			CU_PASS("[SKIPPED] Target does not support "
				"COMPARE_AND_WRITE. Skipping test");
			return;
		}
		CU_ASSERT_EQUAL(ret, 0);

		logging(LOG_VERBOSE, "Read %d blocks at LBA:0 and verify "
			"they are all 'B'", i);
		ret = read16(sd, NULL, 0, i * block_size,
			     block_size, 0, 0, 0, 0, 0, buf,
			     EXPECT_STATUS_GOOD);
		CU_ASSERT_EQUAL(ret, 0);

		for (j = 0; j < i * block_size; j++) {
			if (buf[j] != 'B') {
				logging(LOG_VERBOSE, "[FAILED] Data did not "
					"read back as 'B' (buf[%d] = %#02x)",
					j, buf[j]);
				CU_FAIL("Block was not written correctly");
				return;
			}
		}
	}


	logging(LOG_VERBOSE, "Test COMPARE_AND_WRITE of 1-256 blocks at the "
		"end of the LUN");
	for (i = 1; i < 256; i++) {
		logging(LOG_VERBOSE, "Write %d blocks of 'A' at LBA:%" PRIu64,
			i, num_blocks - i);
		memset(buf, 'A', 2 * i * block_size);
		if (maximum_transfer_length && maximum_transfer_length < i) {
			break;
		}
		ret = write16(sd, num_blocks - i, i * block_size,
			      block_size, 0, 0, 0, 0, 0, buf,
			      EXPECT_STATUS_GOOD);
		CU_ASSERT_EQUAL(ret, 0);

		if (i > maxbl) {
			logging(LOG_VERBOSE, "Number of blocks %d is greater than "
				"BlockLimits.MaximumCompareAndWriteLength(%d). "
				"Command should fail with INVALID_FIELD_IN_CDB",
				i, maxbl);
			ret = compareandwrite(sd, 0,
					      buf, 2 * i * block_size,
					      block_size, 0, 0, 0, 0,
					      EXPECT_INVALID_FIELD_IN_CDB);
			CU_ASSERT_EQUAL(ret, 0);

			continue;
		}
		memset(buf + i * block_size, 'B', i * block_size);

		logging(LOG_VERBOSE, "Overwrite %d blocks with 'B' "
			"at LBA:%" PRIu64 " (if they all contain 'A')",
			i, num_blocks - i);
		ret = compareandwrite(sd, num_blocks - i,
				      buf, 2 * i * block_size, block_size,
				      0, 0, 0, 0,
				      EXPECT_STATUS_GOOD);
		CU_ASSERT_EQUAL(ret, 0);

		logging(LOG_VERBOSE, "Read %d blocks at LBA:%" PRIu64 
			" and verify they are all 'B'",
			i, num_blocks - i);
		ret = read16(sd, NULL, num_blocks - i, i * block_size,
			     block_size, 0, 0, 0, 0, 0, buf,
			     EXPECT_STATUS_GOOD);
		CU_ASSERT_EQUAL(ret, 0);

		for (j = 0; j < i * block_size; j++) {
			if (buf[j] != 'B') {
				logging(LOG_VERBOSE, "[FAILED] Data did not "
					"read back as 'B' (buf[%d] = %#02x)",
					j, buf[j]);
				CU_FAIL("Block was not written correctly");
				return;
			}
		}
	}
}
