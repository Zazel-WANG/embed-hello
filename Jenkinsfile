pipeline {
    agent any
    stages {
        stage('Cross Compile') {
            steps {
                sh 'echo "编译" > /tmp/current-stage.txt'
                sh '''
                    VER=$(git rev-parse --short HEAD)
                    cd workspace && make clean && make ARCH=arm64 VERSION=${VER}
                '''
            }
        }
        stage('Unit Test') {
            steps {
                sh 'echo "单元测试" > /tmp/current-stage.txt'
                sh 'cd workspace && make test ARCH=arm64'
            }
        }
        stage('Verify Binary') {
            steps {
                sh 'echo "二进制验证" > /tmp/current-stage.txt'
                sh '''
                    VER=$(git rev-parse --short HEAD)
                    cd workspace && make verify ARCH=arm64 VERSION=${VER}
                '''
            }
        }
        stage('Deploy to LubanCat') {
            when {
                branch 'main'
            }
            steps {
                sh 'echo "部署运行" > /tmp/current-stage.txt'
                sh '''
                    VER=$(git rev-parse --short HEAD)
                    cd workspace
                    ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "powershell -Command \"New-Item -ItemType Directory -Force -Path E:/AI-helper/projects/embed-hello/workspace/build\""
                    scp -o StrictHostKeyChecking=no build/hello-${VER} HUAWEI@10.0.0.2:E:/AI-helper/projects/embed-hello/workspace/build/hello-${VER}
                    ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "scp -o StrictHostKeyChecking=no E:/AI-helper/projects/embed-hello/workspace/build/hello-${VER} cat@192.168.137.100:/home/cat/deploy/hello && ssh cat@192.168.137.100 chmod +x /home/cat/deploy/hello && ssh cat@192.168.137.100 /home/cat/deploy/hello"
                '''
            }
        }
    }
    post {
        success {
            sh '''
                VER=$(git rev-parse --short HEAD)
                { echo "SUCCESS"; echo "-"; git log -1 --format=%s; } | base64 -w0 > /tmp/build-msg.b64
                ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "powershell -Command \"New-Item -ItemType Directory -Force -Path E:/AI-helper/projects/embed-hello/workspace/build\""
                scp -o StrictHostKeyChecking=no /tmp/build-msg.b64 HUAWEI@10.0.0.2:E:/AI-helper/projects/embed-hello/workspace/build/build-status.txt
                ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "powershell E:\\AI-helper\\projects\\cicd\\workspace\\deploy\\notify-build.ps1"
            '''
        }
        failure {
            sh '''
                { echo "FAILED"; cat /tmp/current-stage.txt; git log -1 --format=%s; } | base64 -w0 > /tmp/build-msg.b64
                ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "powershell -Command \"New-Item -ItemType Directory -Force -Path E:/AI-helper/projects/embed-hello/workspace/build\""
                scp -o StrictHostKeyChecking=no /tmp/build-msg.b64 HUAWEI@10.0.0.2:E:/AI-helper/projects/embed-hello/workspace/build/build-status.txt
                ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "powershell E:\\AI-helper\\projects\\cicd\\workspace\\deploy\\notify-build.ps1"
            '''
        }
    }
}
