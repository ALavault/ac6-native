// Validates that imported LF_ARRAY datatypes use the byte length encoded in the PDB.

import ghidra.app.script.GhidraScript;
import xexloaderwv.PDBFile;
import xexloaderwv.TypeRecord;
import xexloaderwv.TypeRecord.LeafRecordKind;

public class ValidatePdbArrayLengths extends GhidraScript {

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 1) {
			throw new IllegalArgumentException("Usage: ValidatePdbArrayLengths.java <path-to-pdb>");
		}

		String pdbPath = args[0];
		printf("Loading PDB for array validation: %s%n", pdbPath);

		PDBFile pdb = new PDBFile(pdbPath, monitor, currentProgram);
		pdb.tpi.ImportTypeRecords(currentProgram, monitor);

		int checked = 0;
		int skipped = 0;
		int mismatches = 0;
		StringBuilder details = new StringBuilder();

		for (TypeRecord rec : pdb.tpi.typeRecords) {
			if (rec.kind != LeafRecordKind.LF_ARRAY || !(rec.record instanceof TypeRecord.LR_Array)) {
				continue;
			}

			TypeRecord.LR_Array arr = (TypeRecord.LR_Array) rec.record;
			if (arr.dataType == null) {
				skipped++;
				continue;
			}

			long expectedLength = arr.val.val_long;
			int actualLength = arr.dataType.getLength();
			checked++;

			if (actualLength != expectedLength) {
				mismatches++;
				if (details.length() < 4000) {
					details.append(String.format(
						"typeID=0x%X name=%s expectedBytes=%d actualBytes=%d elemType=0x%X%n",
						rec.typeID, arr.name, expectedLength, actualLength, arr.elemtype));
				}
			}
		}

		printf("Array validation summary: checked=%d skipped=%d mismatches=%d%n",
			checked, skipped, mismatches);

		if (mismatches > 0) {
			throw new RuntimeException("Found array length mismatches:\n" + details);
		}
	}
}
