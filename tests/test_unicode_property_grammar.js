/* ECMA-262 UnicodeMatchProperty Tables 65/66/67 exact spelling and aliases. */
function reject(pattern, flags) {
    try {
        new RegExp(pattern, flags);
    } catch (error) {
        if (error instanceof SyntaxError)
            return;
        throw error;
    }
    throw new Error("Accepted invalid Unicode property escape: " + pattern);
}
for (const name of ["IDS_Unary_Operator", "IDSU", "Modifier_Combining_Mark", "MCM",
                    "ID_Compat_Math_Start", "ID_Compat_Math_Continue", "InCB",
                    "Alphabetic ", "alphabetic", "ALPHABETIC", "Other_Math"])
    for (const flags of ["u", "v"])
        reject("\\p{" + name + "}", flags);
for (const name of ["ASCII", "Any", "Assigned", "Alphabetic", "Alpha", "AHex",
                    "Lower", "Upper", "space", "CWCF", "IDSB", "IDST"])
    for (const flags of ["u", "v"])
        new RegExp("\\p{" + name + "}", flags);
for (const pattern of ["\\p{script=Latin}", "\\p{Script=latin}",
                       "\\p{GeneralCategory=Lu}", "\\p{gc=uppercase_letter}",
                       "\\p{Block=Basic_Latin}", "\\p{Age=1.1}"])
    reject(pattern, "u");
