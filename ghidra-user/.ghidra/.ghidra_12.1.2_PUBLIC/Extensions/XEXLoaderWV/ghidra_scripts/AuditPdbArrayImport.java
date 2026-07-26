// Audits LF_ARRAY import outcomes so skipped arrays can be classified by reason.

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.DataType;
import xexloaderwv.PDBFile;
import xexloaderwv.TPIStream;
import xexloaderwv.TypeRecord;
import xexloaderwv.TypeRecord.LeafRecordKind;

public class AuditPdbArrayImport extends GhidraScript {

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 1) {
			throw new IllegalArgumentException("Usage: AuditPdbArrayImport.java <path-to-pdb>");
		}

		String pdbPath = args[0];
		printf("Loading PDB for array audit: %s%n", pdbPath);

		PDBFile pdb = new PDBFile(pdbPath, monitor, currentProgram);
		pdb.tpi.ImportTypeRecords(currentProgram, monitor);

		int imported = 0;
		int skippedZeroLength = 0;
		int skippedMissingElementType = 0;
		int skippedInvalidElementLength = 0;
		int skippedNonDivisible = 0;
		int skippedUnexpected = 0;
		StringBuilder details = new StringBuilder();

		for (TypeRecord rec : pdb.tpi.typeRecords) {
			if (rec.kind != LeafRecordKind.LF_ARRAY || !(rec.record instanceof TypeRecord.LR_Array)) {
				continue;
			}

			TypeRecord.LR_Array arr = (TypeRecord.LR_Array) rec.record;
			if (arr.dataType != null) {
				imported++;
				continue;
			}

			long expectedLength = arr.val.val_long;
			DataType elementType = null;
			try {
				elementType = pdb.tpi.GetDataTypeByIndex(arr.elemtype);
			}
			catch (Exception ex) {
				elementType = null;
			}

			String reason;
			if (expectedLength == 0) {
				skippedZeroLength++;
				reason = "zero-length";
			}
			else if (elementType == null) {
				skippedMissingElementType++;
				reason = "missing-element-type";
			}
			else if (elementType.getLength() <= 0) {
				skippedInvalidElementLength++;
				reason = "invalid-element-length";
			}
			else if ((expectedLength % elementType.getLength()) != 0) {
				skippedNonDivisible++;
				reason = "non-divisible";
			}
			else {
				skippedUnexpected++;
				reason = "unexpected";
			}

			if (details.length() < 6000) {
				String typeName = pdb.tpi.GetDataTypeNameByIndex(arr.elemtype);
				String elementName = typeName == null ? "<null>" : typeName;
				String elementLength = elementType == null ? "<null>" : Integer.toString(elementType.getLength());
				details.append(String.format(
					"typeID=0x%X name=%s expectedBytes=%d elemType=0x%X elemName=%s elemLen=%s reason=%s%n",
					rec.typeID, arr.name, expectedLength, arr.elemtype, elementName, elementLength, reason));
			}
		}

		printf(
			"Array audit summary: imported=%d skippedZeroLength=%d skippedMissingElementType=%d skippedInvalidElementLength=%d skippedNonDivisible=%d skippedUnexpected=%d%n",
			imported,
			skippedZeroLength,
			skippedMissingElementType,
			skippedInvalidElementLength,
			skippedNonDivisible,
			skippedUnexpected);
		if (details.length() > 0) {
			printf("%s", details.toString());
		}

		if (skippedUnexpected > 0) {
			throw new RuntimeException("Unexpected skipped arrays were found.");
		}
	}
}
