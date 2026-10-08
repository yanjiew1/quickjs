/* Optional cases for tests/microbench.js test_list, enabled Intl only.
 * Call register_intl_number_duration_benchmarks(test_list) before selection. */
function register_intl_number_duration_benchmarks(test_list) {
    if (typeof Intl !== "object" || !Intl.NumberFormat || !Intl.DurationFormat) return;
    const number = new Intl.NumberFormat("en-US");
    const exact = new Intl.NumberFormat("en-US", {useGrouping:false,minimumFractionDigits:3,maximumFractionDigits:3});
    const digital = new Intl.DurationFormat("en-US", {style:"digital",fractionalDigits:3});
    const textual = new Intl.DurationFormat("en-US", {style:"long"});
    const format = number.format;
    let sink;
    function intl_number_format(n) { for (let i=0;i<n;i++) sink=format(1234567.89); return n; }
    function intl_number_bigint(n) { for (let i=0;i<n;i++) sink=exact.format(900719925474099312345n); return n; }
    function intl_number_decimal(n) { for (let i=0;i<n;i++) sink=exact.format("9007199254740993.125"); return n; }
    function intl_number_parts(n) { for (let i=0;i<n;i++) sink=number.formatToParts(1234567.89); return n; }
    function intl_number_range(n) { for (let i=0;i<n;i++) sink=number.formatRange(12345.6,12346.7); return n; }
    function intl_number_constructor(n) { for (let i=0;i<n;i++) sink=new Intl.NumberFormat("en-US").format(12345.6); return n; }
    function intl_duration_digital(n) { const v={hours:1,minutes:2,seconds:3,milliseconds:456}; for (let i=0;i<n;i++) sink=digital.format(v); return n; }
    function intl_duration_textual(n) { const v={years:1,days:2,hours:3}; for (let i=0;i<n;i++) sink=textual.format(v); return n; }
    test_list.push(intl_number_format,intl_number_bigint,intl_number_decimal,intl_number_parts,
        intl_number_range,intl_number_constructor,intl_duration_digital,intl_duration_textual);
}
