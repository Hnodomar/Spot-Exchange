from subprocess import Popen
import time

def startServerBenchmark():
    server = Popen(['../../build/tradeserver', '9001', 'output.txt'])
    time.sleep(1)
    dataplatform = Popen(['../../build/dataplatform', '127.0.0.1', '9001'])
    time.sleep(1)
    commands = []
    for i in range(1, 5):
        commands.append(['../../build/tests/benchmark/serverbencher', '{}'.format(i)])
    procs = [Popen(i) for i in commands]
    server.wait()
    for p in procs:
        p.kill()
    dataplatform.kill()

if __name__ == '__main__':
    startServerBenchmark()
