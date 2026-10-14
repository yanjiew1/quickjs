/* Root concatenates the pinned proposed witness-runtime.js before this mock.
 * Names below are derived from pinned CLDR 48.2 en.xml, not observed output. */
const Test262Error = class extends Error {};
function assert(passed) { if (passed !== true) throw new Test262Error("mock assertion"); }
assert._isSameValue = Object.is;
assert.sameValue = function(a,b) { if (!Object.is(a,b)) throw new Test262Error("mock mismatch"); };
function check(v,m) { if(!v) throw new Error(m); }
const monthNames = {"common": {"M01": "Tishri", "M02": "Heshvan", "M03": "Kislev", "M04": "Tevet", "M05": "Shevat", "M06": "Adar", "M07": "Nisan", "M08": "Iyar", "M09": "Sivan", "M10": "Tamuz", "M11": "Av", "M12": "Elul"}, "leap": {"M01": "Tishri", "M02": "Heshvan", "M03": "Kislev", "M04": "Tevet", "M05": "Shevat", "M05L": "Adar I", "M06": "Adar II", "M07": "Nisan", "M08": "Iyar", "M09": "Sivan", "M10": "Tamuz", "M11": "Av", "M12": "Elul"}};
let damage = "";
class DateFormatMock {
  constructor(locale,options) {this.locale=locale;this.options=options;}
  resolvedOptions() {
    const r={locale:"en",numberingSystem:"latn",timeZone:"UTC",year:"numeric",day:"numeric",
      calendar:this.options.calendar,month:this.options.calendar === "hebrew" ? "short" : "numeric"};
    if (damage === "resolved-month") r.month = r.calendar === "hebrew" ? "numeric" : "short";
    if (damage === "arbitrary-month") r.month="long";
    if (damage === "resolved-calendar") r.calendar="gregory";
    if (damage === "resolved-year") r.year="2-digit";
    if (damage === "resolved-day") r.day="2-digit";
    if (damage === "resolved-timezone") r.timeZone="America/New_York";
    if (damage === "resolved-numbering") r.numberingSystem="arab";
    return r;
  }
  formatToParts(date) {
    let month=this.options.calendar === "hebrew" ? monthNames[date.inLeapYear ? "leap" : "common"][date.monthCode] :
      String(Number(date.monthCode.slice(1,3))) + (date.monthCode.endsWith("L") ? "bis" : "");
    if(damage === "ordinal-month") month="6";
    if(damage === "unknown-month") month="unreviewed";
    if(damage === "swapped-adar") month=month === "Adar I" ? "Adar II" : "Adar I";
    if(damage === "wrong-leap-marker") month=month.replace("bis","");
    let parts=[{type:"month",value:month},{type:"literal",value:"/"},
      {type:"day",value:String(date.day+(damage === "wrong-day" ? 1 : 0))},
      {type:"literal",value:"/"},{type:"year",value:String(date.year+(damage === "wrong-year" ? 1 : 0))}];
    if(damage === "missing-year") parts=parts.filter(p=>p.type!=="year");
    if(damage === "related-year") parts[4].type="relatedYear";
    if(damage === "duplicate-year") parts.push({type:"year",value:String(date.year)});
    if(damage === "duplicate-month") parts.push({type:"month",value:month});
    return parts;
  }
  formatRange() {return "unused";}
  get format() {return date=>this.formatToParts(date).map(p=>p.value).join("")+(damage === "wrong-join" ? "bad" : "");}
}
const Intl={DateTimeFormat:DateFormatMock};
function configuration(sites) {
  const data={source_files:{"src/intl/date-native.c":"H2"},hebrewMonthNames:monthNames};
  const review={source_review_complete:true,conformance_accepted:false,semantic_adjudication_complete:false,
    engine_sha256:"e",source_tree_sha256:"s",profile_sha256:"p",trace_proof_sha256:"t",reviewed_guard_sites:{guard:"lunisolar-selected-U"}};
  return {fixture:"test/intl402/DateTimeFormat/prototype/formatToParts/compare-to-temporal-lunisolar.js",
    fixture_sha256:"f",variant:"strict",_config_sha256:"c",profile_sha256:"p",engine_sha256:"e",source_tree_sha256:"s",max_events:2000,sites,
    profile:{defaults:{DateTimeFormat:{locale:"en-US",timeZone:"UTC",numberingSystem:"latn"},Collator:{collation:"default"}}},premise_data:data,
    premise_bindings:{engine_sha256:"e",source_tree_sha256:"s",profile_sha256:"p",current_source_files:data.source_files,
      source_guard_review:review,trace_proof_sha256:"t",root_guard_review_sha256:"r".repeat(64)}};
}
function scenario(corruption="",calendar="hebrew",code="M05L",leap=true) {
  damage=corruption;
  const sites={ctor:{kind:"constructor",source:"new Intl.DateTimeFormat(locale,options)",source_sha256:"c"},
    parts:{kind:"call",source:"formatter.formatToParts(epochMilliseconds)",source_sha256:"p"},
    guard:{kind:"assertion",source:"assert.sameValue(fields.year,date.year)",source_sha256:"g",assertion_method:"sameValue",
      premise_guard:"lunisolar-selected-U",premise_parameters:{},continue_exact_data_assertion:true}};
  const events=[],w=createOriginalWitness(configuration(sites),line=>events.push(JSON.parse(line)));
  const date={calendarId:calendar,year:5809,day:30,monthCode:code,inLeapYear:leap};
  const f=w.call("ctor",()=>new Intl.DateTimeFormat(w.arg("ctor",0,"en"),w.arg("ctor",1,{calendar,timeZone:"UTC",year:"numeric",month:"numeric",day:"numeric"})));
  const parts=w.call("parts",()=>w.recv("parts",f).formatToParts(w.arg("parts",0,date)));
  const part=parts.find(p=>p.type==="year"||p.type==="relatedYear");
  w.premiseAssertion("guard",assert.sameValue,assert,()=>({kind:"lunisolar-selected-U",formatter:f,date,parts,part}))(Number(part&&part.value),date.year);
  w.finish();
  check(events.some(e=>e.kind === "premise-guard" && e.permit_snapshot_continuation === false),"Present Hebrew year was waived");
  check(events.every(e=>e.conformance_accepted === false && e.semantic_adjudication_complete === false),"Unexpected conformance authority");
}
let positives=0,rejections=0;
for(const leap of [false,true]) for(const code of Object.keys(monthNames[leap ? "leap" : "common"])) {scenario("","hebrew",code,leap);positives++;}
for(const cal of ["chinese","dangi"])for(const code of ["M01","M05L","M12"]) {scenario("",cal,code,true);positives++;}
for(const bad of ["resolved-month","arbitrary-month","resolved-calendar","resolved-year","resolved-day","resolved-timezone","resolved-numbering",
  "ordinal-month","unknown-month","swapped-adar","wrong-day","wrong-year","missing-year","related-year","duplicate-year","duplicate-month","wrong-join"]) {
  let rejected=false;try{scenario(bad);}catch(e){rejected=true;}check(rejected,"Corrupted Hebrew obligation accepted: "+bad);rejections++;
}
for(const [code,leap] of [["M05L",false],["M13",true],["M06L",true],["M00",true]]) {
  let rejected=false;try{scenario("","hebrew",code,leap);}catch(e){rejected=true;}check(rejected,"Wrong Hebrew month domain accepted");rejections++;
}
for(const cal of ["chinese","dangi"])for(const bad of ["resolved-month","wrong-leap-marker"]) {
  let rejected=false;try{scenario(bad,cal,"M05L",true);}catch(e){rejected=true;}check(rejected,"Chinese/Dangi month duty weakened");rejections++;
}
print("H2 lunisolar guard: "+positives+" positive and "+rejections+" rejection scenarios.");
