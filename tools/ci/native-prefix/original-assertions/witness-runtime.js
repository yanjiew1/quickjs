/* Isolated diagnostics only. Ordinary Test262 run and failures remain unchanged. */
function createOriginalWitness(config, emit) {
  "use strict";
  const N = { apply: Reflect.apply, descriptors: Object.getOwnPropertyDescriptors,
    descriptor: Object.getOwnPropertyDescriptor, keys: Reflect.ownKeys,
    prototype: Object.getPrototypeOf, array: Array.isArray, stringify: JSON.stringify,
    own: Object.prototype.hasOwnProperty, same: assert._isSameValue, error: Test262Error };
  const original = assert, sites = config.sites, counts = Object.create(null);
  const resolvedMethods = Object.create(null);
  for (const name of ["Collator", "DateTimeFormat", "NumberFormat", "PluralRules", "ListFormat", "RelativeTimeFormat", "DurationFormat", "Segmenter"]) {
    const C = Intl[name];
    if (typeof C === "function") {
      const d = N.descriptor(C.prototype, "resolvedOptions");
      if (d && typeof d.value === "function") resolvedMethods[name] = d.value;
    }
  }
  const operations = [], calls = [], origins = new WeakMap(), capturedCalls = new Map(), originalNativeCalls = [];
  const dateMilliseconds = Date.prototype.getMilliseconds;
  const dateRangeParts = typeof Intl.DateTimeFormat === "function" ? N.descriptor(Intl.DateTimeFormat.prototype, "formatRangeToParts") : null;
  const continued = new Set(), failedMandatory = new Set();
  let sequence = 0, dataMismatches = 0, totalized = 0, closed = false, splitBaseline;
  function known(id) {
    if (!N.apply(N.own, sites, [id])) throw new Error("Unknown witness site: " + id);
    return sites[id];
  }
  function snapshot(value, seen, depth) {
    depth = depth || 0;
    if (depth > 32) throw new Error("Witness snapshot depth exceeded");
    const t = typeof value;
    if (value === null || t === "undefined") return {kind: value === null ? "null" : t};
    if (t === "string") {
      const utf16 = [], codepoints = [];
      for (let i = 0; i < value.length; i++) utf16.push(value.charCodeAt(i));
      for (const ch of value) codepoints.push(ch.codePointAt(0));
      return {kind: t, value, utf16, codepoints};
    }
    if (t === "number") return {kind: t, value: value === 0 && 1 / value < 0 ? "-0" :
      Number.isNaN(value) ? "NaN" : value === Infinity ? "+Infinity" : value === -Infinity ? "-Infinity" : String(value)};
    if (t === "boolean") return {kind: t, value};
    if (t === "bigint") return {kind: t, value: String(value)};
    if (t === "function" || t === "symbol") return {kind: t, value: String(value), complete: false};
    if (origins.has(value) && !origins.get(value).dataArray) return {kind: "call-reference", call_event: origins.get(value).event};
    seen = seen || new Set();
    if (seen.has(value)) return {kind: "cycle", complete: false};
    seen.add(value);
    const descriptors = N.descriptors(value), properties = [];
    for (const key of N.keys(descriptors)) {
      const d = descriptors[key], data = N.apply(N.own, d, ["value"]);
      properties.push({key: snapshot(key, seen, depth + 1), enumerable: d.enumerable,
        configurable: d.configurable, writable: data ? d.writable : null,
        value: data ? snapshot(d.value, seen, depth + 1) : {kind: "accessor", complete: false}});
    }
    seen.delete(value);
    return {kind: N.array(value) ? "array" : "object", properties};
  }
  function fullData(s) {
    return s && s.complete !== false && !["call-reference", "function", "symbol", "accessor", "cycle"].includes(s.kind) &&
      (!s.properties || s.properties.every(p => fullData(p.key) && fullData(p.value)));
  }
  function write(kind, id, values) {
    if (closed || sequence >= config.max_events) throw new Error("Witness closed or event bound exceeded");
    if (id !== null) known(id);
    const event = {schema: "native-original-assertion-trace-v2", fixture: config.fixture,
      fixture_sha256: config.fixture_sha256, variant: config.variant, config_sha256: config._config_sha256,
      profile_sha256: config.profile_sha256, engine_sha256: config.engine_sha256,
      source_tree_sha256: config.source_tree_sha256, sequence: ++sequence, kind,
      site: id, site_source_sha256: id === null ? null : sites[id].source_sha256,
      conformance_accepted: false, semantic_adjudication_complete: false};
    for (const key of Object.keys(values || {})) event[key] = values[key];
    emit(N.stringify(event));
    return event.sequence;
  }
  function begin(id, operation, payload) {
    const parent = operations.length ? operations[operations.length - 1].event : null;
    const event = write("enter", id, Object.assign({operation, parent}, payload));
    const frame = {id, operation, event}; operations.push(frame); return frame;
  }
  function end(frame, payload) {
    if (operations[operations.length - 1] !== frame) throw new Error("Witness operation stack imbalance");
    write("exit", frame.id, Object.assign({operation: frame.operation, entry: frame.event}, payload));
    operations.pop(); counts[frame.id] = (counts[frame.id] || 0) + 1;
  }
  function thrown(error) {
    const own = error !== null && (typeof error === "object" || typeof error === "function") ? N.descriptors(error) : {};
    return {test262_error: error instanceof N.error,
      message: own.message && typeof own.message.value === "string" ? own.message.value : null};
  }
  function requireInvariant(id, name, passed, detail) {
    write("invariant", id, {name, passed, detail: detail === undefined ? null : snapshot(detail), mandatory: true});
    if (!passed) {failedMandatory.add(id); throw new Error("Mandatory witness invariant failed: " + name);}
  }
  function mismatch(method, args) {
    if (method === "boolean") return args[0] !== true;
    if (method === "sameValue") return !N.same(args[0], args[1]);
    if (method === "notSameValue") return N.same(args[0], args[1]);
    if (method === "compareArray") return N.array(args[0]) && N.array(args[1]) &&
      (args[0].length !== args[1].length || args[0].some((v, i) => !N.same(v, args[1][i])));
    // Original unchanged deepEqual on fully captured plain string-valued parts has no callback or getter path.
    return method === "deepEqual";
  }
  function assertion(id, fn, receiver, forceMandatory, premiseToken, originalPartLeaf) {
    const s = known(id);
    if (s.kind !== "assertion") throw new Error("Wrong assertion site kind");
    return function(...args) {
      const argsSnapshot = args.map(v => snapshot(v));
      const frame = begin(id, "assertion", {arguments: argsSnapshot, mandatory_override: forceMandatory === true, premise_guard_event: premiseToken ? premiseToken.event : null, ...(originalPartLeaf === undefined ? {} : {original_part_leaf: originalPartLeaf})});
      let result;
      try { result = N.apply(fn, receiver, args); }
      catch (error) {
        const allowed = forceMandatory !== true && s.continue_exact_data_assertion === true &&
          (!s.premise_guard || !!premiseToken && premiseTokens.has(premiseToken) && premiseToken.id === id && premiseToken.permit === true) &&
          error instanceof N.error && fn === (s.assertion_method === "boolean" ? original : original[s.assertion_method]) &&
          ["sameValue", "notSameValue", "compareArray", "deepEqual", "boolean"].includes(s.assertion_method) &&
          argsSnapshot.every(fullData) && mismatch(s.assertion_method, args) &&
          (s.assertion_method !== "deepEqual" || s.unchanged_deep_equal_purity_review_complete === true);
        if (allowed) {dataMismatches++; continued.add(id);} else failedMandatory.add(id);
        end(frame, {completion: "throw", original_assertion_passed: false, continued: allowed, error: thrown(error)});
        if (!allowed) throw error;
        return undefined; // Exact unchanged harness pass return for reviewed data comparators.
      }
      end(frame, {completion: "normal", original_assertion_passed: true, continued: false, result: snapshot(result)});
      return result;
    };
  }
  const PART_TYPES = ["literal", "integer", "group", "decimal", "fraction", "plusSign", "minusSign", "percentSign", "currency",
    "compact", "exponentInteger", "exponentMinusSign", "exponentSeparator", "unit", "nan", "infinity", "approximatelySign",
    "unknown", "weekday", "era", "year", "relatedYear", "yearName", "month", "day", "dayPeriod", "hour", "minute", "second",
    "fractionalSecond", "timeZoneName", "element"];
  function partsShape(id, parts, range, family) {
    requireInvariant(id, "parts-array", N.array(parts));
    if (config.fixture.includes("/NumberFormat/") || config.fixture.includes("/DateTimeFormat/"))
      requireInvariant(id, "number-date-parts-nonempty", parts.length > 0);
    for (let i = 0; i < parts.length; i++) {
      const p = parts[i];
      requireInvariant(id, "part-object-prototype", p !== null && typeof p === "object" && N.prototype(p) === Object.prototype);
      const fields = range ? ["type", "value", "source"] : ["type", "value"];
      for (const field of fields) {
        const d = N.descriptor(p, field);
        requireInvariant(id, "part-data-property-" + field, !!d && N.apply(N.own, d, ["value"]) &&
          typeof d.value === "string" && d.writable === true && d.enumerable === true && d.configurable === true, d);
      }
      requireInvariant(id, "part-type-domain", PART_TYPES.includes(p.type), p.type);
      if (range) requireInvariant(id, "part-source-domain", ["startRange", "endRange", "shared"].includes(p.source), p.source);
      if (family === "ListFormat") requireInvariant(id, "list-part-type-domain", ["element", "literal"].includes(p.type), p.type);
    }
  }
  function call(id, thunk) {
    const s = known(id);
    if (!["call", "constructor"].includes(s.kind)) throw new Error("Wrong native call site kind");
    const op = begin(id, s.kind, {}), frame = {id, args: [], rawArgs: [], receiver: null, rawReceiver: null};
    calls.push(frame);
    let result, constructorResolved = null;
    try { result = thunk(); }
    catch (error) {
      end(op, {completion: "throw", arguments: frame.args, receiver: frame.receiver, error: thrown(error), continued: false});
      throw error;
    } finally {
      if (calls.pop() !== frame) throw new Error("Witness native frame imbalance");
    }
    try {
    const event = write("value", id, {operation_entry: op.event, role: "native-result", value: snapshot(result)});
    capturedCalls.set(id,{result,event,rawArgs:frame.rawArgs,rawReceiver:frame.rawReceiver});
    originalNativeCalls.push({result,event,source:s.source,rawArgs:frame.rawArgs,rawReceiver:frame.rawReceiver});
    if (/\.(?:format|formatRange|toLocaleString)\s*\(/.test(s.source)) {
      requireInvariant(id, "formatted-output-string", typeof result === "string");
      if (!config.fixture.includes("/ListFormat/")) requireInvariant(id, "formatted-output-nonempty", result.length > 0);
    }
    if (config.split_invariant_baseline_site && /\.format\s*\(/.test(s.source))
      requireInvariant(id, "split-prototype-mutation-output-invariant", result === splitBaseline);
    if (/\.format(?:Range)?ToParts\s*\(/.test(s.source)) {
      const range = /\.formatRangeToParts\s*\(/.test(s.source), family = config.fixture.includes("/ListFormat/") ? "ListFormat" : "Intl";
      partsShape(id, result, range, family);
      if (range && config.fixture.includes("/DateTimeFormat/") && frame.rawArgs.length === 2 && N.same(frame.rawArgs[0], frame.rawArgs[1]))
        requireInvariant(id, "identical-date-range-shared-source", result.every(p => p.source === "shared"));
    }
    if (/\.select\s*\(/.test(s.source) && config.fixture.includes("/PluralRules/"))
      requireInvariant(id, "plural-category-domain", ["zero", "one", "two", "few", "many", "other"].includes(result), result);
    if (/\.resolvedOptions\s*\(/.test(s.source) && config.fixture.includes("/PluralRules/") && result.pluralCategories) {
      const order = ["zero", "one", "two", "few", "many", "other"], cats = result.pluralCategories;
      requireInvariant(id, "plural-categories-order-domain", N.array(cats) && cats.every((v, i) => order.includes(v) &&
        (i === 0 || order.indexOf(cats[i - 1]) < order.indexOf(v))));
    }
    if (config.fractional_precision_guard && /\.formatRange(?:ToParts)?\s*\(/.test(s.source)) {
      const owner = origins.get(frame.rawReceiver);
      requireInvariant(id, "fractional-formatter-origin", !!owner && !!owner.resolvedOptions && owner.rawArgs.length >= 2);
      const options = owner.rawArgs[1], desc = N.descriptor(options, "fractionalSecondDigits");
      const requested = desc ? desc.value : undefined, resolved = owner.resolvedOptions;
      requireInvariant(id, "fractional-requested-domain", requested === undefined || [1, 2, 3].includes(requested));
      requireInvariant(id, "fractional-resolved-precision", resolved.fractionalSecondDigits === requested);
      requireInvariant(id, "fractional-selected-numbering", resolved.numberingSystem === "latn");
      const p = N.array(result) ? result : N.apply(dateRangeParts.value, frame.rawReceiver, frame.rawArgs);
      partsShape(id, p, true, "Intl");
      write("value", id, {operation_entry: op.event, role: "auxiliary-fractional-range-parts", auxiliary: true, value: snapshot(p)});
      if (typeof result === "string") requireInvariant(id, "fractional-format-parts-consistency", p.map(x => x.value).join("") === result);
      const fraction = p.filter(x => x.type === "fractionalSecond");
      if (requested === undefined) requireInvariant(id, "no-unrequested-fractional-field", fraction.length === 0);
      else {
        const expected = frame.rawArgs.map(x => String(Math.floor(N.apply(dateMilliseconds, x, []) / Math.pow(10, 3 - requested))).padStart(requested, "0"));
        requireInvariant(id, "fractional-field-width", fraction.every(x => x.value.length === requested));
        if (expected[0] === expected[1]) requireInvariant(id, "shared-fractional-value", fraction.length === 1 && fraction[0].source === "shared" && fraction[0].value === expected[0]);
        else requireInvariant(id, "range-fractional-values-and-source", fraction.length === 2 &&
          fraction.filter(x => x.source === "startRange" && x.value === expected[0]).length === 1 &&
          fraction.filter(x => x.source === "endRange" && x.value === expected[1]).length === 1);
      }
    }
    if (s.kind === "constructor") {
      const named = /new\s+Intl\.([A-Za-z]+)/.exec(s.source);
      if (named && named[1] !== "Locale") {
        const method = resolvedMethods[named[1]];
        requireInvariant(id, "constructor-resolvedOptions-available", typeof method === "function");
        const resolved = N.apply(method, result, []); constructorResolved = resolved;
        requireInvariant(id, "resolvedOptions-ordinary-object", resolved !== null && typeof resolved === "object" && N.prototype(resolved) === Object.prototype);
        write("value", id, {operation_entry: op.event, role: "constructor-resolved-options", auxiliary: true, value: snapshot(resolved)});
      }
    }
    if (result !== null && typeof result === "object" && !N.array(result)) origins.set(result, {event, source: s.source, rawArgs: frame.rawArgs, rawReceiver: frame.rawReceiver, resolvedOptions: constructorResolved});
    // Parts arrays remain fully serializable and additionally carry their original input identity privately.
    if (N.array(result)) origins.set(result, {event, source: s.source, rawArgs: frame.rawArgs, rawReceiver: frame.rawReceiver, dataArray: true});
    end(op, {completion: "normal", arguments: frame.args, receiver: frame.receiver, result_event: event});
    return result;
    } catch (error) {
      if (operations[operations.length - 1] === op)
        end(op, {completion: "throw", diagnostic_guard_failure: true, original_native_completion: "normal",
          arguments: frame.args, receiver: frame.receiver, error: thrown(error), continued: false});
      throw error;
    }
  }
  function current(id) {
    const frame = calls[calls.length - 1];
    if (!frame || frame.id !== id) throw new Error("Witness native argument frame missing");
    return frame;
  }
  function arg(id, index, value) {const f = current(id); f.args[index] = snapshot(value); f.rawArgs[index] = value; return value;}
  function recv(id, value) {const f = current(id); f.receiver = snapshot(value); f.rawReceiver = value; return value;}
  function value(id, v) {write("value", id, {role: "original-operand", value: snapshot(v)}); counts[id] = (counts[id] || 0) + 1; return v;}
  function helper(id, args, thunk) {
    const frame = begin(id, "helper", {arguments: Array.from(args, v => snapshot(v))});
    let result;
    try {result = thunk();}
    catch (error) {end(frame, {completion: "throw", error: thrown(error), continued: false}); throw error;}
    end(frame, {completion: "normal", result: snapshot(result), continued: false}); return result;
  }
  function dataArraySnapshot(a) {
    const origin = origins.get(a);
    if (origin && origin.dataArray) {origins.delete(a); const s = snapshot(a); origins.set(a, origin); return s;}
    return snapshot(a);
  }
  function parts(id, actual, expected, spec, message) {
    const actualSnapshot = dataArraySnapshot(actual), expectedSnapshot = dataArraySnapshot(expected);
    const frame = begin(id, "aggregate", {actual: actualSnapshot, expected: expectedSnapshot, message: snapshot(message),
      adapter_mode: spec.mode, original_helper_body_sha256: spec.original_helper_body_sha256});
    requireInvariant(id, "complete-pure-part-arrays", fullData(actualSnapshot) && fullData(expectedSnapshot));
    partsShape(id, actual, actual.length > 0 && N.apply(N.own, actual[0], ["source"]), config.fixture.includes("/ListFormat/") ? "ListFormat" : "Intl");
    requireInvariant(id, "expected-array", N.array(expected));
    const origin = origins.get(actual);
    const sameInputs = origin && /formatRangeToParts/.test(origin.source) && origin.rawArgs.length === 2 && N.same(origin.rawArgs[0], origin.rawArgs[1]);
    const mandatory = spec.mode === "mandatory" || spec.mode === "date-equality-invocation" && !!sameInputs;
    for (const shape of spec.shape_sites) assertion(shape.site, original.sameValue, original, true)(N.array(shape.argument === "actual" ? actual : expected), true, message);
    assertion(spec.length_site, original.sameValue, original, mandatory || spec.mode === "list-mandatory-elements")(actual.length, expected.length, message);
    const bound = Math.max(actual.length, expected.length);
    for (let i = 0; i < bound; i++) {
      const a = actual[i], e = expected[i];
      for (const field of spec.fields) {
        const available = a !== undefined && e !== undefined;
        const must = mandatory || spec.mode === "list-mandatory-elements" && (field.field === "type" || a && a.type === "element");
        if (!available) {
          const allowed = !must && known(field.site).continue_exact_data_assertion === true;
          write("original-obligation", field.site, {aggregate_entry: frame.event, index: i, field: field.field,
            actual: snapshot(a && a[field.field]), expected: snapshot(e && e[field.field]), original_evaluation_performed: false,
            original_iteration: i < actual.length, expected_entry_present: e !== undefined, actual_entry_present: a !== undefined,
            totalized_reviewed_data_comparison: allowed, original_assertion_passed: false, continued: allowed});
          counts[field.site] = (counts[field.site] || 0) + 1; totalized++;
          if (!allowed) {failedMandatory.add(field.site); throw new Error("Mandatory aggregate expression unavailable");}
          continued.add(field.site); dataMismatches++;
        } else if (!must && field.field === "value" && known(field.site).premise_guard === "unsupported-es-selected-unit-list") {
          const token = premiseCheck(field.site, {kind:"unsupported-es-selected-unit-list", formatter:origin && origin.rawReceiver,
            parts:actual, expected, index:i}, [a.value, e.value, message]);
          assertion(field.site, original.sameValue, original, false, token, {aggregate_entry:frame.event, index:i, field:"value"})(a.value, e.value, message);
        } else if (field.field === "value" && known(field.site).premise_guard === "unsupported-es-selected-unit-list") {
          assertion(field.site, original.sameValue, original, must, undefined, {aggregate_entry:frame.event, index:i, field:"value"})(a.value, e.value, message);
        } else assertion(field.site, original.sameValue, original, must)(a[field.field], e[field.field], message);
      }
      if (a !== undefined) {
        for (const site of spec.prototype_sites) assertion(site, original.sameValue, original, true)(N.prototype(a), Object.prototype, message);
        for (const d of spec.descriptors) assertion(d.site, verifyProperty, undefined, true)(a, d.field, {enumerable: true, writable: true, configurable: true});
      }
    }
    end(frame, {completion: "normal", continued: false, original_pass_return: snapshot(undefined)});
    return undefined;
  }



  // Exact source-site conditional capture. These checks never grant normative acceptance.
  const premiseTokens = new WeakSet(), fieldPremises = new WeakMap();
  const nativeTypeError = TypeError, unavailableReads = new WeakMap();
  const dateRangeFormat = N.descriptor(Intl.DateTimeFormat.prototype, "formatRange").value;
  const datePartsMethod = N.descriptor(Intl.DateTimeFormat.prototype, "formatToParts").value;
  const dateFormatGetter = N.descriptor(Intl.DateTimeFormat.prototype, "format").get;
  const dateLocalHour = Date.prototype.getHours, dateLocalMinute = Date.prototype.getMinutes;
  function premiseBinding(id) {
    const s = known(id), b = config.premise_bindings, data = config.premise_data;
    const review = b && b.source_guard_review;
    requireInvariant(id, "premise-root-binding", !!b && !!data && !!review &&
      b.engine_sha256 === config.engine_sha256 && b.source_tree_sha256 === config.source_tree_sha256 &&
      b.profile_sha256 === config.profile_sha256 && review.engine_sha256 === config.engine_sha256 &&
      review.source_tree_sha256 === config.source_tree_sha256 && review.profile_sha256 === config.profile_sha256 &&
      review.source_review_complete === true && review.conformance_accepted === false &&
      review.semantic_adjudication_complete === false && review.reviewed_guard_sites[id] === s.premise_guard &&
      typeof b.trace_proof_sha256 === "string" && b.trace_proof_sha256 === review.trace_proof_sha256 &&
      typeof b.root_guard_review_sha256 === "string" && b.root_guard_review_sha256.length === 64);
    requireInvariant(id, "premise-exact-production-source", !!b.current_source_files &&
      Object.keys(data.source_files).every(k => b.current_source_files[k] === data.source_files[k]));
    requireInvariant(id, "premise-effective-profile", !!config.profile && !!config.profile.defaults &&
      config.profile.defaults.DateTimeFormat.timeZone === "UTC" &&
      config.profile.defaults.DateTimeFormat.locale === "en-US" &&
      config.profile.defaults.DateTimeFormat.numberingSystem === "latn" &&
      config.profile.defaults.Collator.collation === "default");
    return data;
  }
  let premiseAuxiliaryReceipts = [];
  function auxiliary(id, role, thunk) {
    const v = thunk();
    const event = write("value", id, {role: "auxiliary-premise-data", auxiliary_role: role, auxiliary: true, value: snapshot(v)});
    premiseAuxiliaryReceipts.push({event, role});
    return v;
  }
  function selected(id, formatter) {
    const o = origins.get(formatter);
    requireInvariant(id, "premise-original-formatter-origin", !!o && !!o.resolvedOptions &&
      /new\s+Intl\.DateTimeFormat/.test(o.source));
    return o.resolvedOptions;
  }
  function selectedEnglish(id, resolved) {
    requireInvariant(id, "premise-selected-English-latn", /^en(?:-US)?(?:-u-ca-(?:chinese|dangi))?$/.test(resolved.locale) && resolved.numberingSystem === "latn");
  }
  function numericField(id, p, type, expected, source) {
    const a = p.filter(x => x.type === type && (source === undefined || x.source === source));
    requireInvariant(id, "premise-numeric-field-" + type, a.length === 1 && /^-?[0-9]+$/.test(a[0].value) &&
      Number(a[0].value) === expected, {expected, actual: a});
  }
  function namedYear(id, parts, year, source, data) {
    requireInvariant(id, "premise-related-year-integer", Number.isInteger(year));
    const y = parts.filter(x => x.type === "yearName" && (source === undefined || x.source === source));
    const expected = data.yearNames[((year - 4) % 60 + 60) % 60];
    requireInvariant(id, "premise-selected-U-correct-calendar-name", y.length === 1 && y[0].value === expected,
      {calendar_year: year, expected, actual: y, source});
  }
  function dayperiodEvidence(id, formatter, inputs, width, data) {
    const r = selected(id, formatter); selectedEnglish(id, r);
    const constructor = origins.get(formatter), options = constructor.rawArgs[1], descriptors = N.descriptors(options);
    requireInvariant(id,"premise-original-dayperiod-options",constructor.rawArgs[0] === "en" &&
      N.prototype(options) === Object.prototype && Object.keys(descriptors).every(k => ["dayPeriod","hour"].includes(k) && N.apply(N.own,descriptors[k],["value"])) &&
      options.dayPeriod === width && (options.hour === undefined || options.hour === "numeric"));
    requireInvariant(id, "premise-real-h-B-selected-fields", r.calendar === "gregory" && r.dayPeriod === width &&
      r.hour === "numeric" && r.hourCycle === "h12" && r.hour12 === true && r.timeZone === "UTC" && r.minute === undefined && r.second === undefined,
      r);
    requireInvariant(id, "premise-original-24-hour-inputs", N.array(inputs) && inputs.length === 24);
    const points = [];
    for (let h = 0; h < 24; ++h) {
      const hour = N.apply(dateLocalHour, inputs[h], []), minute = N.apply(dateLocalMinute, inputs[h], []);
      requireInvariant(id, "premise-original-hour-order", hour === h && minute === 0, {h, hour, minute});
      const p = auxiliary(id, "dayperiod-parts", () => N.apply(datePartsMethod, formatter, [inputs[h]]));
      partsShape(id, p, false, "Intl");
      requireInvariant(id, "premise-selected-h-B-part-order", p.length === 3 && p[0].type === "hour" &&
        p[1].type === "literal" && p[1].value === " " && p[2].type === "dayPeriod", p);
      numericField(id, p, "hour", h % 12 || 12);
      const name = h === 0 ? "midnight" : h === 12 ? "noon" : h < 12 ? "morning1" : h < 18 ? "afternoon1" : h < 21 ? "evening1" : "night1";
      requireInvariant(id, "premise-source-dayperiod-boundary-name", p[2].value === data.dayPeriodLabels[width][name],
        {h, name, width, expected: data.dayPeriodLabels[width][name], actual: p[2].value});
      const text = auxiliary(id, "dayperiod-format", () => N.apply(N.apply(dateFormatGetter, formatter, []), undefined, [inputs[h]]));
      requireInvariant(id, "premise-format-parts-join", typeof text === "string" && p.map(x => x.value).join("") === text);
      points.push({hour: h, rule: name, dayPeriod: p[2].value, numericHour: p[0].value, text});
    }
    const evidence = {selected: r, points, source_pattern: "h B", requested_dayPeriod: width};
    return evidence;
  }
  function originalDayperiodEvidence(id, c, width, data) {
    const records = originalNativeCalls.filter(o => (o.rawReceiver === c.formatter ||
      c.numericFormatter !== undefined && o.rawReceiver === c.numericFormatter) &&
      /\.format(?:ToParts)?\s*\(/.test(o.source));
    requireInvariant(id, "premise-dayperiod-original-native-results-present", records.length > 0);
    const rows = [];
    for (const o of records) {
      const h = c.inputs.indexOf(o.rawArgs[0]), f = origins.get(o.rawReceiver);
      requireInvariant(id, "premise-dayperiod-original-input-and-formatter-binding", o.rawArgs.length === 1 &&
        h >= 0 && !!f && !!f.resolvedOptions);
      const rule = h === 0 ? "midnight" : h === 12 ? "noon" : h < 12 ? "morning1" : h < 18 ? "afternoon1" : h < 21 ? "evening1" : "night1";
      const expected = data.dayPeriodLabels[width][rule], hour = String(h % 12 || 12);
      let text;
      if (N.array(o.result)) {
        partsShape(id, o.result, false, "Intl");
        requireInvariant(id, "premise-dayperiod-original-part-order", o.result.length === 3 && o.result[0].type === "hour" &&
          o.result[1].type === "literal" && o.result[1].value === " " && o.result[2].type === "dayPeriod");
        numericField(id, o.result, "hour", h % 12 || 12);
        requireInvariant(id, "premise-dayperiod-original-name", o.result[2].value === expected);
        text = auxiliary(id, "dayperiod-format", () => N.apply(N.apply(dateFormatGetter,o.rawReceiver,[]), undefined,o.rawArgs));
        requireInvariant(id, "premise-dayperiod-original-join", o.result.map(p => p.value).join("") === text);
      } else {
        text = o.result;
        requireInvariant(id, "premise-dayperiod-original-string-numeric-name-order", typeof text === "string" && text === hour + " " + expected);
      }
      rows.push({event:o.event, hour:h, formatter_event:f.event, mode:N.array(o.result) ? "parts" : "format", result:o.result, text});
    }
    const base = rows.filter(row => row.formatter_event === origins.get(c.formatter).event);
    if (c.helperParts !== undefined) {
      const o = origins.get(c.helperParts);
      requireInvariant(id, "premise-dayperiod-original-helper-parts-binding", !!o && records.some(r => r.event === o.event && r.result === c.helperParts));
    } else {
      requireInvariant(id, "premise-dayperiod-original-full-observed-inputs", base.length === 24 && base.every((r,h) => r.hour === h));
    }
    requireInvariant(id, "premise-dayperiod-original-observed-values-binding", N.array(c.observedDayPeriods) &&
      c.observedDayPeriods.every((value,h) => base[h] && value === (base[h].mode === "parts" ? base[h].result[0].value : base[h].result)) &&
      c.observedDayPeriods.length === (c.helperParts !== undefined && records[records.length-1].rawReceiver === c.formatter ? base.length - 1 : 24));
    return {rows, observed:c.observedDayPeriods, helper_parts_event:c.helperParts === undefined ? null : origins.get(c.helperParts).event};
  }
  function premiseCheck(id, c, args) {
    premiseAuxiliaryReceipts = [];
    const s = known(id), data = premiseBinding(id);
    requireInvariant(id, "premise-context-kind", !!c && c.kind === s.premise_guard);
    let permit = false, evidence, textForPremiseJoin, timezoneInputs, timezoneEquals;
    if (c.kind === "dayperiod-selected-hB") {
      const width = s.premise_parameters.width;
      evidence = {base: dayperiodEvidence(id, c.formatter, c.inputs, width, data)};
      if (c.numericFormatter !== undefined) evidence.numeric = dayperiodEvidence(id, c.numericFormatter, c.inputs, width, data);
      evidence.original = originalDayperiodEvidence(id,c,width,data);
      permit = true;
    } else if (c.kind === "unmatched-locale-hc-default") {
      const params = s.premise_parameters, captured = capturedCalls.get(params.constructor_site);
      const ctor = captured && origins.get(captured.result), ro = origins.get(c.resolvedOptions);
      requireInvariant(id, "premise-original-unmatched-hc-origin", !!ctor && !!ro &&
        /new\s+Intl\.DateTimeFormat/.test(ctor.source) && /\.resolvedOptions\s*\(/.test(ro.source) &&
        ro.rawReceiver === captured.result && ro.rawArgs.length === 0);
      const requested = ctor.rawArgs[0], options = ctor.rawArgs[1], keys = N.keys(options);
      requireInvariant(id, "premise-original-unmatched-hc-options", ctor.rawArgs.length === 2 &&
        N.prototype(options) === Object.prototype && keys.length === 1 && keys[0] === params.option_mode &&
        N.apply(N.own, N.descriptor(options, params.option_mode), ["value"]) &&
        (params.option_mode === "hour" ? options.hour === "numeric" : ["full","long","medium","short"].includes(options.timeStyle)));
      requireInvariant(id, "premise-original-unmatched-hc-extension", ["h11","h12","h23","h24"].includes(c.requestedCycle) &&
        requested === "de-u-hc-" + c.requestedCycle && data.fallbackHourCycle.requested_base === "de");
      const supported = auxiliary(id,"unmatched-hc-supported",() => Intl.DateTimeFormat.supportedLocalesOf(requested));
      const selected = ctor.resolvedOptions, resolved = {...c.resolvedOptions}, fallback = data.fallbackHourCycle;
      requireInvariant(id, "premise-unmatched-hc-support", N.array(supported) && supported.length === 0);
      requireInvariant(id, "premise-unmatched-hc-selected-default", !!selected && selected.locale === fallback.selected_locale &&
        selected.calendar === "gregory" && selected.numberingSystem === "latn" && selected.timeZone === "UTC" &&
        selected.hourCycle === fallback.hourCycle && selected.hour12 === fallback.hour12 &&
        N.keys(selected).length === N.keys(resolved).length && N.keys(selected).every(k => N.same(selected[k],resolved[k])) &&
        (params.option_mode === "hour" ? selected.hour === "numeric" && selected.timeStyle === undefined : selected.timeStyle === options.timeStyle));
      requireInvariant(id,"premise-unmatched-hc-original-comparison", ["hourCycle","hour12"].includes(params.field) &&
        N.same(args[0], resolved[params.field]) && N.same(args[1], params.field === "hourCycle" ? c.requestedCycle : ["h11","h12"].includes(c.requestedCycle)));
      c.formatter = captured.result;
      evidence = {original_options:{...options},requested,requested_cycle:c.requestedCycle,selected,resolved,supported,compared:args.slice(0,2)};
      permit = true;
    } else if (c.kind === "unsupported-es-selected-unit-list") {
      const partsOrigin = origins.get(c.parts), ctor = c.formatter && origins.get(c.formatter), params = s.premise_parameters;
      requireInvariant(id,"premise-original-selected-list-origin", !!partsOrigin && !!ctor &&
        /\.formatToParts\s*\(/.test(partsOrigin.source) && /new\s+Intl\.ListFormat/.test(ctor.source) &&
        partsOrigin.rawReceiver === c.formatter && partsOrigin.rawArgs.length === 1);
      const options = ctor.rawArgs[1], selected = ctor.resolvedOptions;
      requireInvariant(id,"premise-original-selected-list-options", ctor.rawArgs.length === 2 && ctor.rawArgs[0] === "es-ES" &&
        N.prototype(options) === Object.prototype && N.keys(options).length === 2 && N.keys(options).every(k => ["style","type"].includes(k) && N.apply(N.own,N.descriptor(options,k),["value"])) &&
        options.type === "unit" && options.style === params.style);
      const supported = auxiliary(id,"selected-list-supported",() => Intl.ListFormat.supportedLocalesOf("es-ES"));
      requireInvariant(id,"premise-selected-list-unsupported-fallback", N.array(supported) && supported.length === 0 &&
        selected.locale === "en-US" && selected.type === "unit" && selected.style === params.style &&
        N.keys(selected).length === 3 && N.keys(selected).every(k => ["locale","type","style"].includes(k)));
      const templates = data.selectedUnitListTemplates[params.style];
      requireInvariant(id,"premise-selected-list-source-templates", N.array(templates) && templates.length === 4 && templates.every(t => t === "{0}, {1}"));
      const input = partsOrigin.rawArgs[0], inputOrigin = input && typeof input === "object" ? origins.get(input) : undefined;
      let elements;
      if (typeof input === "string") elements = Array.from(input);
      else if (N.array(input)) elements = input.slice();
      else if (inputOrigin && /\[Symbol\.iterator\]\s*\(/.test(inputOrigin.source) && N.array(inputOrigin.rawReceiver)) elements = inputOrigin.rawReceiver.slice();
      else if (input && typeof input === "object" && N.keys(input).length === 2 && N.keys(input).every(k => ["i","array"].includes(k)) &&
        N.array(input.array) && input.i === input.array.length) elements = input.array.slice();
      requireInvariant(id,"premise-selected-list-original-input-elements", N.array(elements) && elements.every(v => typeof v === "string") &&
        (N.same(input,"foo") ? elements.join("") === "foo" : [[],["foo"],["foo","bar"],["foo","bar","baz"],["foo","bar","baz","quux"]].some(v => v.length === elements.length && v.every((x,i) => x === elements[i]))));
      requireInvariant(id,"premise-selected-list-exact-parts-keys", N.prototype(c.parts) === Array.prototype &&
        c.parts.every(p => N.prototype(p) === Object.prototype && N.keys(p).length === 2 && N.keys(p)[0] === "type" && N.keys(p)[1] === "value"));
      partsShape(id,c.parts,false,"ListFormat");
      const expected = []; elements.forEach((value,i) => {if(i) expected.push({type:"literal",value:", "}); expected.push({type:"element",value});});
      requireInvariant(id,"premise-selected-list-element-order-and-literals", c.parts.length === expected.length &&
        c.parts.every((p,i) => p.type === expected[i].type && p.value === expected[i].value) &&
        c.expected.length === expected.length && c.expected.every((p,i) => p.type === expected[i].type &&
          typeof p.value === "string" && (p.type === "literal" || p.value === expected[i].value)));
      requireInvariant(id,"premise-selected-list-original-literal-leaf", Number.isInteger(c.index) && c.index >= 0 && c.index < c.parts.length &&
        c.parts[c.index].type === "literal" && c.expected[c.index].type === "literal" &&
        args[0] === c.parts[c.index].value && args[1] === c.expected[c.index].value);
      const joined = auxiliary(id,"selected-list-format-join",() => N.apply(N.descriptor(Intl.ListFormat.prototype,"format").value,c.formatter,[elements]));
      requireInvariant(id,"premise-selected-list-same-receiver-join", typeof joined === "string" && joined === c.parts.map(p => p.value).join(""));
      evidence = {selected,original_options:{...options},actual_parts:c.parts,expected_parts:c.expected,elements,index:c.index,supported,joined};
      textForPremiseJoin = joined; permit = true;
    } else if (c.kind === "relatedyear-selected-U") {
      const r = selected(id, c.formatter); selectedEnglish(id, r);
      requireInvariant(id, "premise-selected-lunisolar-calendar", ["chinese", "dangi"].includes(c.calendar) && r.calendar === c.calendar &&
        r.year === "numeric" && r.month === "numeric" && r.day === "numeric" && r.timeZone === "UTC", r);
      partsShape(id, c.parts, c.source !== undefined, "Intl");
      const absent = c.expectedType === "relatedYear" && c.part === undefined;
      if (absent) {
        requireInvariant(id, "premise-no-selected-r-token", !c.parts.some(x => x.type === "relatedYear" &&
          (c.source === undefined || x.source === c.source)));
        namedYear(id, c.parts, c.expectedValue, c.source, data);
      }
      // The original present month/day rows retain exact numeric obligations.
      if (c.expectedType === "month" || c.expectedType === "day") numericField(id, c.parts, c.expectedType, c.expectedValue, c.source);
      const o = origins.get(c.parts);
      requireInvariant(id, "premise-original-parts-origin", !!o && o.rawReceiver === c.formatter &&
        o.rawArgs.length === c.dates.length && o.rawArgs.every((v,i) => v === c.dates[i]));
      const text = auxiliary(id, "calendar-format-join", () => c.source === undefined ?
        N.apply(N.apply(dateFormatGetter, c.formatter, []), undefined, c.dates) :
        N.apply(dateRangeFormat, c.formatter, c.dates));
      requireInvariant(id, "premise-calendar-parts-format-join", typeof text === "string" && c.parts.map(p => p.value).join("") === text);
      textForPremiseJoin = text;
      permit = absent;
      evidence = {calendar: c.calendar, expectedType: c.expectedType, expectedValue: c.expectedValue,
        part: c.part, actual_parts: c.parts, selected: r, source: c.source, selected_U_without_r: absent};
    } else if (c.kind === "lunisolar-selected-U") {
      const r = selected(id, c.formatter); selectedEnglish(id, r);
      const absent = c.parts.find(p => p.type === "year" || p.type === "relatedYear") === undefined;
      const hebrew = c.date.calendarId === "hebrew";
      requireInvariant(id, "premise-temporal-calendar-binding", ["chinese", "dangi", "hebrew"].includes(c.date.calendarId) &&
        r.calendar === c.date.calendarId && r.year === "numeric" &&
        r.month === (hebrew ? "short" : "numeric") && r.day === "numeric" && r.timeZone === "UTC", r);
      partsShape(id, c.parts, false, "Intl");
      if (absent) {
        requireInvariant(id, "premise-no-numeric-year-only-lunisolar", ["chinese", "dangi"].includes(c.date.calendarId));
        namedYear(id, c.parts, c.date.year, undefined, data);
      }
      numericField(id, c.parts, "day", c.date.day);
      if (["chinese", "dangi"].includes(c.date.calendarId)) {
        const m = c.parts.filter(p => p.type === "month"), expected = /^M([0-9]{2})(L?)$/.exec(c.date.monthCode);
        requireInvariant(id, "premise-original-lunisolar-month-code-domain", !!expected);
        requireInvariant(id, "premise-original-lunisolar-month-and-leap-marker", m.length === 1 &&
          Number.parseInt(m[0].value,10) === Number(expected[1]) &&
          (expected[2] === "L" ? /^[0-9]+[^0-9]+$/.test(m[0].value) : /^[0-9]+$/.test(m[0].value)), m);
      } else {
        // CLDR 48.2 English abbreviated Hebrew names are pinned independently;
        // M05L is Adar I, while M06 is Adar or Adar II according to leap year.
        requireInvariant(id, "premise-hebrew-numeric-year-part", !absent && c.part && c.part.type === "year");
        numericField(id, c.parts, "year", c.date.year);
        requireInvariant(id, "premise-hebrew-month-code-and-leap-domain", typeof c.date.inLeapYear === "boolean" &&
          /^M(?:0[1-9]|1[0-2]|05L)$/.test(c.date.monthCode) &&
          (c.date.monthCode !== "M05L" || c.date.inLeapYear));
        const names = data.hebrewMonthNames && data.hebrewMonthNames[c.date.inLeapYear ? "leap" : "common"];
        const expected = names && names[c.date.monthCode], m = c.parts.filter(p => p.type === "month");
        requireInvariant(id, "premise-hebrew-independent-named-month", typeof expected === "string" &&
          m.length === 1 && m[0].value === expected,
          {monthCode:c.date.monthCode, inLeapYear:c.date.inLeapYear, expected, actual:m});
      }
      const o = origins.get(c.parts);
      requireInvariant(id, "premise-original-lunisolar-parts-origin", !!o && o.rawReceiver === c.formatter && o.rawArgs.length === 1);
      const text = auxiliary(id, "lunisolar-format-join", () => N.apply(N.apply(dateFormatGetter,c.formatter,[]), undefined,o.rawArgs));
      requireInvariant(id, "premise-lunisolar-parts-format-join", typeof text === "string" && c.parts.map(p => p.value).join("") === text);
      textForPremiseJoin = text;
      permit = absent;
      evidence = {calendar:c.date.calendarId, calendar_year:c.date.year, monthCode:c.date.monthCode,
        day:c.date.day, hebrew_in_leap_year:hebrew ? c.date.inLeapYear : undefined,
        selected:r, actual_parts:c.parts, numeric_year_part:c.part,
        unavailable_read_event:c.unavailable_read_event, selected_U_without_r:absent};
    } else if (c.kind === "temporal-numeric-hour") {
      const params = s.premise_parameters, expected = params.hour;
      const call = capturedCalls.get(params.call_site);
      requireInvariant(id, "premise-original-hourcycle-call", !!call && call.rawReceiver === c.item && call.result === c.result &&
        call.rawArgs.length === 2 && call.rawArgs[0] === "en" && typeof c.result === "string");
      const options = call.rawArgs[1], ds = N.descriptors(options), key = params.option.split(":")[0];
      requireInvariant(id, "premise-original-hourcycle-options-shape", N.prototype(options) === Object.prototype &&
        Object.keys(ds).every(k => [key,"timeZone"].includes(k) && N.apply(N.own,ds[k],["value"])) &&
        Object.keys(ds).includes(key) && (options.timeZone === undefined || options.timeZone === "UTC"));
      requireInvariant(id, "premise-original-hourcycle-option", c.option === params.option &&
        String(options[key]) === params.option.split(":")[1]);
      const probe = auxiliary(id, "hourcycle-resolved-probe", () => new Intl.DateTimeFormat("en",
        Object.assign({}, options, {hour:"numeric",minute:"numeric",second:"numeric",timeZone:"UTC"})).resolvedOptions());
      selectedEnglish(id, probe);
      requireInvariant(id, "premise-hourcycle-option-resolution", probe.hourCycle === params.hourCycle &&
        probe.hour12 === ["h11","h12"].includes(params.hourCycle) && probe.timeZone === "UTC", probe);
      requireInvariant(id, "premise-original-midnight-temporal-input", c.item.epochNanoseconds !== undefined ?
        c.item.epochNanoseconds === 0n && (c.item.timeZoneId === undefined || c.item.timeZoneId === "UTC") :
        c.item.hour === 0 && c.item.minute === 0 && c.item.second === 0 && c.item.millisecond === 0 &&
        c.item.microsecond === 0 && c.item.nanosecond === 0);
      const matches = Array.from(c.result.matchAll(/(?:^|[^0-9])([0-9]{1,2}):([0-9]{2}):([0-9]{2})(?=[^0-9]|$)/g));
      requireInvariant(id, "premise-original-midnight-numeric-cycle", matches.length === 1 &&
        Number(matches[0][1]) === expected && matches[0][2] === "00" && matches[0][3] === "00",
        {actual:c.result, expected_numeric_hour:expected, requested:c.option});
      requireInvariant(id, "premise-original-midnight-period", !probe.hour12 || /(?:^|[^A-Za-z])AM(?:[^A-Za-z]|$)/.test(c.result));
      permit = true; evidence = {result:c.result, option:c.option, original_options:options, original_call_event:call.event,
        selected:probe, expected_numeric_hour:expected, actual_numeric_hour:Number(matches[0][1]),
        midnight_input:c.item.epochNanoseconds !== undefined ? {epochNanoseconds:String(c.item.epochNanoseconds),timeZoneId:c.item.timeZoneId} :
          {hour:c.item.hour,minute:c.item.minute,second:c.item.second,millisecond:c.item.millisecond,microsecond:c.item.microsecond,nanosecond:c.item.nanosecond}};
    } else if (c.kind === "arEG-fallback-latn") {
      const r = selected(id, c.formatter); selectedEnglish(id, r);
      const supported = auxiliary(id, "ar-EG-supportedLocalesOf", () => Intl.DateTimeFormat.supportedLocalesOf(["ar-EG"], {localeMatcher:"lookup"}));
      requireInvariant(id, "premise-arEG-unsupported-fallback", N.array(supported) && supported.length === 0 && r.calendar === "iso8601");
      const p = auxiliary(id, "extreme-plain-date-parts", () => N.apply(datePartsMethod, c.formatter, [c.date]));
      partsShape(id, p, false, "Intl");
      numericField(id, p, "day", c.date.day); numericField(id, p, "month", c.date.month);
      numericField(id, p, "year", c.date.year <= 0 ? 1 - c.date.year : c.date.year);
      requireInvariant(id, "premise-extreme-date-option-resolution", r.year === "numeric" && r.month === "numeric" && r.day === "numeric" && r.timeZone === "UTC");
      requireInvariant(id, "premise-extreme-date-format-parts-join", p.map(x => x.value).join("") === c.result);
      requireInvariant(id, "premise-extreme-date-source-values", (c.date.year === 275760 && c.date.month === 9 && c.date.day === 13) ||
        (c.date.year === -271821 && c.date.month === 4 && c.date.day === 19));
      permit = true; evidence = {selected: r, result: c.result, actual_parts: p, iso_year: c.date.year, month: c.date.month, day: c.date.day};
    } else if (c.kind === "collator-default-only") {
      const origin = origins.get(c.resolvedOptions), constructor = origin && origins.get(origin.rawReceiver);
      requireInvariant(id, "premise-original-collator-option-origin", !!origin && !!constructor &&
        /\.resolvedOptions\s*\(/.test(origin.source) && /new\s+Intl\.Collator/.test(constructor.source) &&
        constructor.rawArgs[0] === c.locale && constructor.rawArgs[1].collation === c.collation);
      const supported = auxiliary(id, "collator-original-locale-support", () => Intl.Collator.supportedLocalesOf([c.locale], {localeMatcher:"lookup"}));
      const base = c.locale.split("-u-")[0], expectedLocale = base === "en" ? "en" : "en-US";
      requireInvariant(id, "premise-collator-support-policy", ["de", "en"].includes(base) && N.array(supported) &&
        supported.length === (base === "en" ? 1 : 0) &&
        (base !== "en" || supported[0] === c.locale));
      requireInvariant(id, "premise-required-collation-option-extension-resolution", c.resolvedOptions.locale === expectedLocale &&
        c.resolvedOptions.collation === "default" && ["pinyin", "eor", "phonebk"].includes(c.collation) &&
        c.resolvedOptions.usage === "sort" && c.resolvedOptions.numeric === false && c.resolvedOptions.caseFirst === "false" &&
        c.resolvedOptions.sensitivity === "variant" && c.resolvedOptions.ignorePunctuation === false, c.resolvedOptions);
      permit = true; evidence = {locale:c.locale, collation:c.collation, supported, resolved: Object.assign({},c.resolvedOptions), original_resolved_event:origin.event, selected_co:[null]};
    } else if (c.kind === "locale-default-only") {
      const supported = auxiliary(id, "locale-collator-prefix-support", () => Intl.Collator.supportedLocalesOf([c.tag], {localeMatcher:"lookup"}));
      const origin = origins.get(c.collations), constructor = origin && origins.get(origin.rawReceiver);
      requireInvariant(id, "premise-original-locale-collations-origin", !!origin && !!constructor &&
        /\.getCollations\s*\(/.test(origin.source) && /new\s+Intl\.Locale/.test(constructor.source) && constructor.rawArgs[0] === c.tag);
      const a = c.collations;
      requireInvariant(id, "premise-collations-array-shape-domain-order", N.array(a) && a.every((x,i) => typeof x === "string" &&
        x !== "standard" && x !== "search" && (i === 0 || a[i-1] < x)), a);
      requireInvariant(id, "premise-locale-collation-supported-default-only-or-fallback", N.array(supported) &&
        (supported.length === 1 ? c.tag === "en" && a.length === 0 : supported.length === 0 && a.length === 2 && a[0] === "emoji" && a[1] === "eor"),
        {tag:c.tag, supported, collations:a});
      permit = supported.length === 1 && c.tag === "en" && a.length === 0;
      evidence = {tag:c.tag, supported, collations:a, selected_co:permit ? [null] : null};
    } else if (c.kind === "tz-primary-identity") {
      const selectedRecords = config.selected_provider_input ? config.selected_provider_input.records : data.timezoneRecords;
      const a = selectedRecords[c.link], z = selectedRecords[c.zone];
      requireInvariant(id, "premise-known-current-zone-records", !!a && !!z, {link:c.link, zone:c.zone});
      timezoneInputs = [c.link,c.zone].map(zone => originalNativeCalls.filter(o =>
        /new\s+Temporal\.ZonedDateTime/.test(o.source) && String(o.rawArgs[0]) === String(c.epochNs) && o.rawArgs[1] === zone).slice(-1)[0]);
      requireInvariant(id,"premise-original-timezone-constructor-inputs",timezoneInputs.every(o => !!o && o.rawArgs.length === 2));
      const same = a.primary === z.primary;
      if (s.premise_parameters.operation === "equals") {
        timezoneEquals = originalNativeCalls.filter(o => /\.equals\s*\(/.test(o.source) &&
          o.rawReceiver === timezoneInputs[0].result && o.rawArgs.length === 1 && o.rawArgs[0] === timezoneInputs[1].result && o.result === args[0]).slice(-1)[0];
        requireInvariant(id,"premise-original-timezone-equals-origin",!!timezoneEquals);
        requireInvariant(id, "premise-required-primary-identity-equality", args.length >= 2 && args[0] === same && args[1] === true,
          {link:c.link, zone:c.zone, link_primary:a.primary, zone_primary:z.primary, actual:args[0]});
      } else {
        const epoch = String(c.epochNs);
        requireInvariant(id, "premise-offset-epoch-source-coverage", Object.prototype.hasOwnProperty.call(a.offsets, epoch) &&
          Object.prototype.hasOwnProperty.call(z.offsets, epoch));
        requireInvariant(id, "premise-zone-independent-source-offset-values", args[0] === a.offsets[epoch] && args[1] === z.offsets[epoch],
          {link:c.link, zone:c.zone, epochNs:epoch, actual_link:args[0], expected_link:a.offsets[epoch], actual_zone:args[1], expected_zone:z.offsets[epoch]});
      }
      permit = !same; evidence = {link:c.link, zone:c.zone, link_primary:a.primary, zone_primary:z.primary,
        epochNs:String(c.epochNs), operation:s.premise_parameters.operation, actual:args.slice(0,2)};
    } else throw new Error("Unknown source premise guard");
    const formatterOrigin = c.formatter === undefined ? undefined : origins.get(c.formatter);
    const partsOrigin = c.parts === undefined ? undefined : origins.get(c.parts);
    const resultOrigin = c.resolvedOptions !== undefined ? origins.get(c.resolvedOptions) : c.collations !== undefined ? origins.get(c.collations) : undefined;
    evidence.proof = {schema:"native-premise-branch-proof-v2", guard_site:id,
      formatter_event:formatterOrigin ? formatterOrigin.event : null,
      parts_event:partsOrigin ? partsOrigin.event : null,
      original_result_event:resultOrigin ? resultOrigin.event : c.kind === "temporal-numeric-hour" ? capturedCalls.get(s.premise_parameters.call_site).event : c.kind === "tz-primary-identity" ? timezoneEquals ? timezoneEquals.event : null : c.kind === "arEG-fallback-latn" ? originalNativeCalls.filter(o => o.rawReceiver === c.formatter && o.result === c.result && o.rawArgs[0] === c.date).slice(-1)[0].event : null,
      auxiliary_events:premiseAuxiliaryReceipts,
      original_arguments:c.kind === "tz-primary-identity" ? timezoneInputs.map(o => o.event) : partsOrigin ? partsOrigin.rawArgs.map(v => {const o=origins.get(v); return o ? {original_event:o.event} : v;}) : null,
      join_text:c.kind === "relatedyear-selected-U" || c.kind === "lunisolar-selected-U" || c.kind === "unsupported-es-selected-unit-list" ? textForPremiseJoin : null};
    const event = write("premise-guard", id, {guard_id:s.premise_guard, passed:true, permit_snapshot_continuation:permit,
      evidence:snapshot(evidence), root_guard_review_sha256:config.premise_bindings.root_guard_review_sha256});
    const token = {id, event, permit}; premiseTokens.add(token); return token;
  }
  function premiseAssertion(id, fn, receiver, context) {
    return function(...args) {
      const token = premiseCheck(id, context(), args);
      return assertion(id, fn, receiver, false, token)(...args);
    };
  }
  function absentContext(c, prerequisite) {
    if (prerequisite === "relatedyear-selected-U") return c.expectedType === "relatedYear" && c.part === undefined;
    if (prerequisite === "lunisolar-selected-U") return c.part === undefined && ["chinese","dangi"].includes(c.date.calendarId);
    throw new Error("Unknown unavailable premise");
  }
  function observeUnavailable(id, thunk, c, prerequisite, adapterKind) {
    const s = known(id);
    requireInvariant(id, "premise-exact-unavailable-obligation-allowlist", s.unavailable_premise === prerequisite &&
      s.unavailable_adapter_kind === adapterKind && absentContext(c,prerequisite));
    const token = premiseCheck(id,c,[]);
    requireInvariant(id, "premise-unavailable-expression-prerequisite", token.permit === true);
    const parent = operations[operations.length-1];
    requireInvariant(id, "premise-unavailable-original-helper-binding", !!parent && parent.operation === "helper");
    let error, normal = false;
    try { thunk(); normal = true; } catch (e) { error = e; }
    requireInvariant(id, "premise-original-undefined-dereference-threw-TypeError", !normal && error instanceof nativeTypeError && !(error instanceof N.error));
    const event = write("unavailable-obligation",id,{helper_entry:parent.event,guard_event:token.event,
      prerequisite,adapter_kind:adapterKind,original_evaluation_performed:true,original_assertion_invoked:false,
      original_assertion_passed:false,original_abrupt_premise:true,original_error:thrown(error),error_constructor:"TypeError",
      original_component:snapshot(c.part),actual:snapshot(undefined),expected:snapshot(undefined),continued:true,
      source_expression:s.source,all_original_operand_bytes_preserved:true});
    counts[id]=(counts[id]||0)+1;continued.add(id);dataMismatches++;totalized++;
    if (adapterKind === "year-read") unavailableReads.set(c.parts,event);
    return undefined;
  }
  function adaptObligation(id, thunk, context, prerequisite) {
    const c = context();
    if (!absentContext(c,prerequisite)) return thunk();
    return observeUnavailable(id,thunk,c,prerequisite,"assertion-operands");
  }
  function adaptYearRead(id,thunk,context) {
    const c = context();
    if (!absentContext(c,"lunisolar-selected-U")) return thunk();
    return observeUnavailable(id,thunk,c,"lunisolar-selected-U","year-read");
  }
  function lunisolarFields(fields,formatter,date,parts) {
    fieldPremises.set(fields,{kind:"lunisolar-selected-U",formatter,date,parts,
      part:parts.find(p => p.type === "year" || p.type === "relatedYear"),unavailable_read_event:unavailableReads.get(parts)});
    return fields;
  }
  function fieldsPremise(fields,formatter,date) {
    const c=fieldPremises.get(fields);
    if (!c || c.formatter !== formatter || c.date !== date || fields.year === undefined && !c.unavailable_read_event)
      throw new Error("Original numeric year has no typed unavailable premise lineage");
    return c;
  }

  function finish() {
    if (operations.length || calls.length || failedMandatory.size) throw new Error("Original file completion blocked");
    write("completion", null, {original_eof_reached: true, fixture_capture_complete: true, site_counts: counts,
      data_mismatches: dataMismatches, mandatory_failed_site_ids: Array.from(failedMandatory),
      continued_data_site_ids: Array.from(continued), totalized_original_data_obligations: totalized,
      all_original_expressions_evaluated: totalized === 0});
    closed = true;
  }
  write("start", null, {original_entry_reached: true});
  if (config.split_invariant_baseline_site) {
    const formatter = Intl.DateTimeFormat("de", {});
    splitBaseline = formatter.format(86400000);
    requireInvariant(config.split_invariant_baseline_site, "split-baseline-string", typeof splitBaseline === "string");
    write("value", config.split_invariant_baseline_site, {role: "auxiliary-semantic-baseline",
      value: snapshot({formatted: splitBaseline, resolvedOptions: N.apply(resolvedMethods.DateTimeFormat, formatter, [])})});
    counts[config.split_invariant_baseline_site] = 1;
  }
  return {assertion, call, arg, recv, value, helper, parts, finish, premiseAssertion, adaptObligation, adaptYearRead, lunisolarFields, fieldsPremise};
}
