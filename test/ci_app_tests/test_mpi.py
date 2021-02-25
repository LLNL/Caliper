# MPI application tests
# For simplicity, these tests only run on a single rank without mpiexec

import json
import os
import unittest

import calipertest as cat

class CaliperMPITest(unittest.TestCase):
    """ Test MPI init before Caliper init"""

    def test_mpi_before_cali(self):
        target_cmd = [ './ci_test_mpi_before_cali' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'PATH'                    : '/usr/bin', # for ssh/rsh
            'CALI_CHANNEL_FLUSH_ON_EXIT' : 'false',
            'CALI_SERVICES_ENABLE'    : 'event,mpi,mpireport,trace',
            'CALI_MPI_WHITELIST'      : 'all',
            'CALI_MPIREPORT_FILENAME' : 'stdout',
            'CALI_MPIREPORT_CONFIG'   : 'select count() group by function,mpi.function,mpi.rank format cali'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function'       : 'main',
                         'mpi.function'   : 'MPI_Barrier',
                         'count'          : '2',
                         'mpi.rank'       : '0'
            }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main', 'mpi.function' : 'MPI_Bcast',   'count' : '1' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main', 'mpi.function' : 'MPI_Reduce',  'count' : '1' }))

    def test_cali_before_mpi(self):
        target_cmd = [ './ci_test_cali_before_mpi' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'PATH'                    : '/usr/bin', # for ssh/rsh
            'CALI_CHANNEL_FLUSH_ON_EXIT' : 'false',
            'CALI_SERVICES_ENABLE'    : 'event,mpi,mpireport,trace',
            'CALI_MPI_WHITELIST'      : 'all',
            'CALI_MPIREPORT_FILENAME' : 'stdout',
            'CALI_MPIREPORT_CONFIG'   : 'select count() group by function,mpi.function,mpi.rank format cali'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function'       : 'main',
                         'mpi.function'   : 'MPI_Barrier',
                         'count'          : '2',
                         'mpi.rank'       : '0'
            }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main', 'mpi.function' : 'MPI_Bcast',   'count' : '1' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main', 'mpi.function' : 'MPI_Reduce',  'count' : '1' }))

    def test_mpi_whitelist(self):
        target_cmd = [ './ci_test_mpi_before_cali' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'PATH'                    : '/usr/bin', # for ssh/rsh
            'CALI_CHANNEL_FLUSH_ON_EXIT' : 'false',
            'CALI_SERVICES_ENABLE'    : 'event,mpi,mpireport,trace',
            'CALI_MPI_WHITELIST'      : 'MPI_Bcast',
            'CALI_MPIREPORT_FILENAME' : 'stdout',
            'CALI_MPIREPORT_CONFIG'   : 'select count() group by function,mpi.function format cali'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main', 'mpi.function' : 'MPI_Barrier' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main', 'mpi.function' : 'MPI_Bcast', 'count' : '1' }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main', 'mpi.function' : 'MPI_Reduce'  }))

    def test_mpi_blacklist(self):
        target_cmd = [ './ci_test_mpi_before_cali' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'PATH'                    : '/usr/bin', # for ssh/rsh
            'CALI_CHANNEL_FLUSH_ON_EXIT' : 'false',
            'CALI_SERVICES_ENABLE'    : 'event,mpi,mpireport,trace',
            'CALI_MPI_BLACKLIST'      : 'MPI_Barrier,MPI_Reduce',
            'CALI_MPIREPORT_FILENAME' : 'stdout',
            'CALI_MPIREPORT_CONFIG'   : 'select count() group by function,mpi.function format cali'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main', 'mpi.function' : 'MPI_Barrier' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main', 'mpi.function' : 'MPI_Bcast', 'count' : '1' }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main', 'mpi.function' : 'MPI_Reduce'  }))

    def test_mpi_whitelist_and_blacklist(self):
        target_cmd = [ './ci_test_mpi_before_cali' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'PATH'                    : '/usr/bin', # for ssh/rsh
            'CALI_CHANNEL_FLUSH_ON_EXIT' : 'false',
            'CALI_SERVICES_ENABLE'    : 'event,mpi,mpireport,trace',
            'CALI_MPI_WHITELIST'      : 'MPI_Bcast,MPI_Reduce',
            'CALI_MPI_BLACKLIST'      : 'MPI_Reduce',
            'CALI_MPIREPORT_FILENAME' : 'stdout',
            'CALI_MPIREPORT_CONFIG'   : 'select count() group by function,mpi.function format cali'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main', 'mpi.function' : 'MPI_Barrier' }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main', 'mpi.function' : 'MPI_Bcast', 'count' : '1' }))
        self.assertFalse(cat.has_snapshot_with_attributes(
            snapshots, { 'function' : 'main', 'mpi.function' : 'MPI_Reduce'  }))

    def test_mpi_msg_trace(self):
        target_cmd = [ './ci_test_mpi_before_cali' ]
        query_cmd  = [ '../../src/tools/cali-query/cali-query', '-e' ]

        caliper_config = {
            'PATH'                    : '/usr/bin', # for ssh/rsh
            'CALI_LOG_VERBOSITY'      : '0',
            'CALI_SERVICES_ENABLE'    : 'event,mpi,recorder,trace',
            'CALI_MPI_MSG_TRACING'    : 'true',
            'CALI_MPI_WHITELIST'      : 'all',
            'CALI_RECORDER_FILENAME'  : 'stdout'
        }

        query_output = cat.run_test_with_query(target_cmd, query_cmd, caliper_config)
        snapshots = cat.get_snapshots_from_text(query_output)

        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function'          : 'main',
                         'mpi.function'      : 'MPI_Barrier',
                         'mpi.coll.type'     : '1',
                         'mpi.comm.is_world' : 'true'
            }))
        self.assertTrue(cat.has_snapshot_with_attributes(
            snapshots, { 'function'          : 'main',
                         'mpi.function'      : 'MPI_Bcast',
                         'mpi.coll.type'     : '3',
                         'mpi.coll.root'     : '0',
                         'mpi.comm.is_world' : 'true'
            }))
        self.assertTrue(cat.has_snapshot_with_keys(
            snapshots, { 'function', 'mpi.function', 'mpi.coll.type', 'mpi.call.id'
            }))

    def test_mpireport_controller(self):
        target_cmd = [ './ci_test_mpi_before_cali', 'mpi-report' ]

        caliper_config = {
            'PATH'                    : '/usr/bin', # for ssh/rsh
            'CALI_LOG_VERBOSITY'      : '0',
        }

        log_targets = [
            'Function',
            'MPI_Bcast'
        ]

        report_out,_ = cat.run_test(target_cmd, caliper_config)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

    def test_collective_output_channel(self):
        target_cmd = [ './ci_test_collective_output_channel', 'mpi-report' ]

        caliper_config = {
            'PATH'                    : '/usr/bin', # for ssh/rsh
            'CALI_LOG_VERBOSITY'      : '0',
        }

        log_targets = [
            'Function',
            'MPI_Barrier'
        ]

        report_out,_ = cat.run_test(target_cmd, caliper_config)
        lines = report_out.decode().splitlines()

        for target in log_targets:
            for line in lines:
                if target in line:
                    break
            else:
                self.fail('%s not found in log' % target)

if __name__ == "__main__":
    unittest.main()
