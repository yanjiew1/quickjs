"""Bounded postflight source regression tests; no compiler or engine launch."""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] /
                      'tools/ci/native-prefix/original-assertions'))
import selected_timezone_orchestration as subject
from selected_timezone_proof import EPOCH_NS


def tzif(offset=0):
    header = b'TZif3' + b'\0' * 15 + struct.pack('>6I', 0, 0, 0, 0, 1, 4)
    block = header + struct.pack('>iBB', offset, 0, 0) + b'UTC\0'
    return block + block + b'\n\n'


class PostflightTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.labels = {f'Zone{i:03d}': f'Zone{i:03d}' for i in range(450)}
        self.embedded = {name: dict(primary=name, tzif_sha256=subject.sha(tzif()),
            offsets={epoch: 0 for epoch in EPOCH_NS}) for name in self.labels}
        self.primary_patch = patch.object(subject, 'primary_map', return_value=(self.labels, self.embedded))
        self.primary_patch.start()
        self.addCleanup(self.primary_patch.stop)
        self.normal = self.root / 'ordinary'
        self.diag = self.root / 'diagnostic'
        self.probe = self.root / 'probe'
        self.compiler = self.root / 'compiler'
        for path in (self.normal, self.diag, self.probe, self.compiler):
            path.write_bytes(path.name.encode())
        self.bindings = dict(engine_sha256=subject.file_sha(self.normal),
            source_tree_sha256='a'*64, profile_sha256='b'*64,
            actual_initialized_checkout_commit='1'*40)
        self.configuration = self.root / 'compiler.json'
        self.save(self.configuration, dict(diagnostic_configuration={
            'CONFIG_TZ_CAPTURE_SNAPSHOT':'y', 'CONFIG_ICU':'n', 'normal_policy':'system-first'},
            ordinary_engine_sha256=self.bindings['engine_sha256'], source_tree_sha256='a'*64,
            diagnostic_engine_sha256=subject.file_sha(self.diag), probe_sha256=subject.file_sha(self.probe),
            make_args=['make','CONFIG_TZ_CAPTURE_SNAPSHOT=y','CONFIG_ICU=n'],
            system_root='/usr/share/zoneinfo', backend='native',
            compiler_path=str(self.compiler), compiler_sha256=subject.file_sha(self.compiler)))
        self.identity = self.root / 'identity.json'
        self.save(self.identity, dict(candidate_sha='1'*40,
            actual_production_source_map_sha256='a'*64, run_id='123', run_attempt='1'))
        self.diagnostic = dict(schema=subject.DIAG_SCHEMA, normal_engine_sha256=self.bindings['engine_sha256'],
            source_tree_sha256='a'*64, profile_sha256='b'*64,
            engine_sha256=subject.file_sha(self.diag), qjs=str(self.diag),
            probe=str(self.probe), probe_sha256=subject.file_sha(self.probe),
            compiler_configuration=str(self.configuration), compiler_configuration_sha256=subject.file_sha(self.configuration),
            execution_identity=str(self.identity), execution_identity_sha256=subject.file_sha(self.identity),
            hosted_identity=dict(workflow='mock',job='mock',run_id='123',run_attempt='1'),
            fixture_sha256='d'*64, normal_policy='system-first', system_root='/usr/share/zoneinfo',
            conformance_accepted=False, semantic_adjudication_complete=False)
        self.bindings['selected_provider_diagnostic'] = self.diagnostic
        self.save(self.root / 'HOSTED-RENDER-PLAN.json', dict(fixtures=[dict(path=subject.TZ_FIXTURE,
            official_original_sha256='d'*64)]))
        self.before = self.root / 'before'
        self.after = self.root / 'after'
        for directory, pid in ((self.before, 11), (self.after, 22)):
            directory.mkdir(mode=0o700)
            rows=[]
            for seq, name in enumerate(sorted(self.labels), 1):
                blob=f'snapshot-{seq:04d}.tzif'
                (directory / blob).write_bytes(tzif())
                rows.append(dict(schema='qjs-selected-tzif-record-v1',pid=pid,sequence=seq,
                    identifier=name,primary=name,origin='system',source_status=0,
                    system_root='/usr/share/zoneinfo',blob=blob,size=len(tzif())))
            (directory / 'selected-timezones.ndjson').write_text(''.join(json.dumps(row)+'\n' for row in rows))
        self.before_path=self.root/'before-command.json'
        self.after_path=self.root/'after-command.json'
        self.capture_path=self.root/'capture-command.json'
        self.before_command=self.command(self.before,11,0,1)
        self.after_command=self.command(self.after,22,4,5)
        self.capture_command=self.command(self.before,33,2,3)
        self.capture_command.update(argv=['python3','capture.py','--qjs',str(self.normal)],
            engine_sha256=self.bindings['engine_sha256'], ordinary_engine_sha256=self.bindings['engine_sha256'])
        self.save(self.before_path,self.before_command)
        self.save(self.after_path,self.after_command)
        self.save(self.capture_path,self.capture_command)
        self.diagnostic.update(preflight_directory=str(self.before),preflight_command=str(self.before_path))
        preproof=subject.prepare_input(self.root,self.bindings,self.diagnostic,self.before_command,self.before)
        prepath=self.root/'before-proof.json';self.save(prepath,preproof)
        self.diagnostic.update(preflight_input_proof=str(prepath),preflight_input_proof_sha256=subject.file_sha(prepath))
        self.postflight=dict(capture_directory=str(self.after),command_receipt=str(self.after_path),
            input_proof=str(self.root/'after-proof.json'),input_proof_sha256=None)
        self.seal_after()

    def save(self,path,value):
        Path(path).write_text(json.dumps(value,indent=2)+'\n')

    def command(self,directory,pid,start,end):
        observer={'QJS_TZ_CAPTURE_DIRECTORY':str(directory)}
        return dict(argv=[str(self.probe)]+sorted(self.labels),cwd=str(self.root),pid=pid,process_group=pid,
            lifecycle_closed=True,returncode=0,timed_out=False,interrupted=False,output_limited=False,
            started_utc=f'2026-10-12T00:00:0{start}+00:00',finished_utc=f'2026-10-12T00:00:0{end}+00:00',
            engine_sha256=subject.file_sha(self.diag),source_tree_sha256='a'*64,profile_sha256='b'*64,
            effective_env_sha256='e'*64,provider_environment_sha256='f'*64,observer_environment=observer,
            observer_environment_sha256=subject.canonical(observer),
            effective_profile_environment={'PREFIX_BACKEND':'native','TZ':'UTC'})

    def seal_after(self):
        derived=dict(self.diagnostic,preflight_directory=str(self.after),preflight_command=str(self.after_path))
        value=subject.prepare_input(self.root,self.bindings,derived,
            json.loads(self.after_path.read_bytes()),self.after)
        value['role']='postflight-selected-provider-input-proof-v1'
        self.save(self.postflight['input_proof'],value)
        self.postflight['input_proof_sha256']=subject.file_sha(self.postflight['input_proof'])

    def compare(self):
        return subject.postflight_receipt(self.root,self.bindings,self.postflight,self.capture_path)

    def test_positive_complete_endpoints_without_continuous_claim(self):
        value=self.compare()
        self.assertEqual(value['compared_primary_count'],450)
        self.assertTrue(value['selected_provider_endpoints_equal'])
        self.assertFalse(value['continuous_monitoring'])
        self.assertFalse(value['conformance_accepted'])
        self.assertEqual(value['original_capture_command_sha256'],subject.file_sha(self.capture_path))
        self.assertEqual(value['provider_environment_sha256'],self.capture_command['provider_environment_sha256'])
        self.assertEqual(value['effective_profile_environment'],self.capture_command['effective_profile_environment'])

    def test_authenticated_receipt_rebuilds_exact_input(self):
        path=self.root/'comparison.json';self.save(path,dict(binding=self.postflight,comparison=self.compare()))
        self.assertEqual(subject.authenticated_postflight(self.root,self.bindings,path,self.capture_path),self.compare())
        document=json.loads(path.read_bytes());document['comparison']['compared_primary_count']=449
        self.save(path,document)
        with self.assertRaises(ValueError): subject.authenticated_postflight(self.root,self.bindings,path,self.capture_path)

    def test_changed_provider_bytes_cannot_be_resealed_into_success(self):
        (self.after/'snapshot-0001.tzif').write_bytes(tzif(3600));self.seal_after()
        with self.assertRaisesRegex(ValueError,'Provider changed'):self.compare()

    def test_origin_status_change_cannot_be_resealed_into_success(self):
        path=self.after/'selected-timezones.ndjson';rows=[json.loads(line) for line in path.read_text().splitlines()]
        rows[0].update(origin='embedded',source_status=2)
        path.write_text(''.join(json.dumps(row)+'\n' for row in rows));self.seal_after()
        with self.assertRaisesRegex(ValueError,'Provider changed'):self.compare()

    def test_wrong_root_rejected(self):
        self.diagnostic['system_root']='/wrong/root'
        with self.assertRaises(ValueError):self.compare()

    def test_changed_compiler_bytes_rejected(self):
        self.compiler.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'compiler bytes'):self.compare()

    def test_changed_ordinary_engine_bytes_rejected(self):
        self.normal.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'Ordinary engine bytes'):self.compare()

    def test_changed_diagnostic_engine_bytes_rejected(self):
        self.diag.write_bytes(b'changed')
        with self.assertRaises(ValueError):self.compare()

    def test_changed_probe_bytes_rejected(self):
        self.probe.write_bytes(b'changed')
        with self.assertRaises(ValueError):self.compare()

    def test_wrong_map_and_profile_rejected(self):
        for field in ('source_tree_sha256','profile_sha256'):
            old=self.after_command[field];self.after_command[field]='f'*64;self.save(self.after_path,self.after_command)
            with self.assertRaises(ValueError):self.compare()
            self.after_command[field]=old;self.save(self.after_path,self.after_command)

    def test_wrong_engine_role_rejected(self):
        self.capture_command['engine_sha256']=subject.file_sha(self.diag);self.save(self.capture_path,self.capture_command)
        with self.assertRaisesRegex(ValueError,'engine differs'):self.compare()

    def test_failed_partial_capture_never_qualifies(self):
        for changes in (dict(returncode=1),dict(lifecycle_closed=False),dict(timed_out=True),
                        dict(interrupted=True),dict(output_limited=True),dict(process_group=999)):
            self.save(self.capture_path,dict(self.capture_command,**changes))
            with self.assertRaises(ValueError):self.compare()

    def test_missing_capture_fields_rejected(self):
        for field in ('started_utc','finished_utc','pid','ordinary_engine_sha256','source_tree_sha256','profile_sha256'):
            value=dict(self.capture_command);value.pop(field);self.save(self.capture_path,value)
            with self.assertRaises(ValueError):self.compare()

    def test_observation_order_rejected(self):
        self.after_command['started_utc']='2026-10-12T00:00:02+00:00';self.save(self.after_path,self.after_command)
        with self.assertRaisesRegex(ValueError,'bracket'):self.compare()

    def test_changed_profile_environment_rejected(self):
        self.after_command['effective_profile_environment']['TZ']='different';self.save(self.after_path,self.after_command)
        with self.assertRaisesRegex(ValueError,'effective profile'):self.compare()

    def test_changed_capture_timezone_environment_rejected(self):
        self.capture_command['effective_profile_environment']['TZ']='different'
        self.save(self.capture_path,self.capture_command)
        with self.assertRaisesRegex(ValueError,'effective profile'):self.compare()

    def test_changed_capture_provider_environment_digest_rejected(self):
        self.capture_command['provider_environment_sha256']='0'*64
        self.save(self.capture_path,self.capture_command)
        with self.assertRaisesRegex(ValueError,'effective profile'):self.compare()

    def test_missing_capture_environment_fields_rejected(self):
        for fields in (('effective_profile_environment',),('provider_environment_sha256',),
                       ('effective_profile_environment','provider_environment_sha256')):
            value=dict(self.capture_command)
            for field in fields:value.pop(field)
            self.save(self.capture_path,value)
            with self.assertRaisesRegex(ValueError,'effective profile'):self.compare()

    def test_changed_nonobserver_environment_rejected(self):
        self.after_command['provider_environment_sha256']='0'*64;self.save(self.after_path,self.after_command)
        with self.assertRaisesRegex(ValueError,'effective profile'):self.compare()

    def test_missing_postflight_manifest_rejected(self):
        (self.after/'selected-timezones.ndjson').unlink()
        with self.assertRaises(ValueError):self.compare()

    def test_tampered_postflight_proof_rejected(self):
        Path(self.postflight['input_proof']).write_text('{}\n')
        with self.assertRaisesRegex(ValueError,'retained actual bytes'):self.compare()

    def test_postflight_primary_coverage_is_exact(self):
        path=self.after/'selected-timezones.ndjson';path.write_text('\n'.join(path.read_text().splitlines()[:-1])+'\n')
        with self.assertRaises(ValueError):self.compare()

    def test_missing_binding_and_extra_fields_rejected(self):
        self.postflight['future_acceptance']=True
        with self.assertRaisesRegex(ValueError,'exact selected provider'):self.compare()


if __name__=='__main__':unittest.main()
