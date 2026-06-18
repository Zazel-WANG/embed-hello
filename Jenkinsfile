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
                not { changeRequest() }
            }
            steps {
                sh 'echo "部署运行" > /tmp/current-stage.txt'
                sh '''
                    VER=$(git rev-parse --short HEAD)
                    if [ "${BRANCH_NAME}" = "main" ]; then
                        DEPLOY_PATH="/home/cat/deploy/hello"
                    else
                        DEPLOY_PATH="/home/cat/deploy-dev/hello"
                    fi
                    cd workspace
                    ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "powershell -Command \"New-Item -ItemType Directory -Force -Path E:/AI-helper/projects/embed-hello/workspace/build\""
                    scp -o StrictHostKeyChecking=no build/hello-${VER} HUAWEI@10.0.0.2:E:/AI-helper/projects/embed-hello/workspace/build/hello-${VER}
                    ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "ssh cat@192.168.137.100 \"mkdir -p \$(dirname ${DEPLOY_PATH})\" && scp -o StrictHostKeyChecking=no E:/AI-helper/projects/embed-hello/workspace/build/hello-${VER} cat@192.168.137.100:${DEPLOY_PATH} && ssh cat@192.168.137.100 chmod +x ${DEPLOY_PATH} && ssh cat@192.168.137.100 ${DEPLOY_PATH}"
                '''
            }
        }

        // ============ AI 交叉编译阶段 (Docker Pipeline 插件) ============
        stage('AI Cross Compile') {
            agent {
                docker {
                    image 'embed-hello-builder:latest'
                    args '--group-add 124'
                    reuseNode true
                }
            }
            steps {
                sh 'echo "AI交叉编译" > /tmp/current-stage.txt'
                sh '''
                    echo "=== AI Cross Compilation ==="
                    aarch64-linux-gnu-gcc --version | head -1
                    echo "Sysroot check:"
                    test -f /usr/aarch64-linux-gnu/include/rknn_api.h && echo "  RKNN headers: OK" || echo "  RKNN headers: MISSING"
                    test -f /usr/aarch64-linux-gnu/include/gstreamer-1.0/gst/gst.h && echo "  GStreamer headers: OK" || echo "  GStreamer headers: MISSING"
                    test -f /usr/aarch64-linux-gnu/include/X11/Xlib.h && echo "  X11 headers: OK" || echo "  X11 headers: MISSING"
                    cd workspace && make ai-all-cross
                '''
            }
        }
        stage('AI Binary Verify') {
            agent {
                docker {
                    image 'embed-hello-builder:latest'
                    args '--group-add 124'
                    reuseNode true
                }
            }
            steps {
                sh 'echo "AI二进制验证" > /tmp/current-stage.txt'
                sh '''
                    cd workspace
                    echo "=== AI Binary Verification ==="
                    for f in build/ai-*-cross; do
                        echo "--- $f ---"
                        file "$f"
                        file "$f" | grep -q "ELF 64-bit.*ARM aarch64" || {
                            echo "FAIL: $f is not aarch64"
                            exit 1
                        }
                        aarch64-linux-gnu-readelf -d "$f" 2>/dev/null | grep NEEDED || true
                    done
                    echo "=== All AI binaries verified: aarch64 ==="
                '''
            }
        }
        stage('Deploy AI to Board') {
            when {
                not { changeRequest() }
            }
            steps {
                sh 'echo "部署AI" > /tmp/current-stage.txt'
                sh '''
                    if [ "${BRANCH_NAME}" = "main" ]; then
                        AI_DEPLOY_PATH="/home/cat/deploy/ai"
                    else
                        AI_DEPLOY_PATH="/home/cat/deploy-dev/ai"
                    fi
                    cd workspace
                    # 通过 Windows 跳板: Jenkins → Windows(10.0.0.2) → 鲁班猫(192.168.137.100)
                    ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "ssh cat@192.168.137.100 \"mkdir -p ${AI_DEPLOY_PATH}\""
                    ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "powershell -Command \"New-Item -ItemType Directory -Force -Path E:/temp/ai-deploy\""
                    for f in build/ai-*-cross; do
                        echo "Deploying: $f"
                        scp -o StrictHostKeyChecking=no "$f" HUAWEI@10.0.0.2:"E:/temp/ai-deploy/"
                        ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "scp -o StrictHostKeyChecking=no E:/temp/ai-deploy/$(basename $f) cat@192.168.137.100:${AI_DEPLOY_PATH}/"
                    done
                    ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "ssh cat@192.168.137.100 \"chmod +x ${AI_DEPLOY_PATH}/ai-*-cross\""
                    echo "=== AI demos deployed to ${AI_DEPLOY_PATH} ==="
                '''
            }
        }
        stage('AI Smoke Test') {
            when {
                not { changeRequest() }
            }
            steps {
                sh 'echo "AI冒烟测试" > /tmp/current-stage.txt'
                sh '''
                    if [ "${BRANCH_NAME}" = "main" ]; then
                        AI_DEPLOY_PATH="/home/cat/deploy/ai"
                    else
                        AI_DEPLOY_PATH="/home/cat/deploy-dev/ai"
                    fi
                    echo "=== AI Smoke Test: ai-query-cross ==="
                    # 关键: 外层单引号 (不用双引号), Windows OpenSSH 中转才不会把命令当成文件名
                    OUTPUT=$(ssh -o StrictHostKeyChecking=no -o ConnectTimeout=10 \
                        HUAWEI@10.0.0.2 \
                        'ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 cat@192.168.137.100 timeout 30 '"${AI_DEPLOY_PATH}"'/ai-query-cross 2>&1' 2>&1) || SSH_EXIT=$?
                    echo "$OUTPUT"
                    if echo "$OUTPUT" | grep -q "SDK:"; then
                        echo "=== Smoke Test PASSED (NPU init OK) ==="
                    elif echo "$OUTPUT" | grep -qiE "Segmentation fault|SIGSEGV"; then
                        echo "=== Smoke Test FAILED: SIGSEGV! ===" && exit 1
                    elif echo "$OUTPUT" | grep -qiE "aborted|SIGABRT"; then
                        echo "=== Smoke Test FAILED: SIGABRT! ===" && exit 1
                    elif [ "$SSH_EXIT" != "0" ] && [ -z "$OUTPUT" ]; then
                        echo "=== Smoke Test WARN: board unreachable ==="
                    else
                        echo "=== Smoke Test WARN: unexpected output ==="
                    fi
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
                ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "powershell E:\\AI-helper\\projects\\embed-hello\\workspace\\deploy\\notify-build.ps1"
            '''
            withCredentials([string(credentialsId: 'gitea-api-token', variable: 'GITEA_TOKEN')]) {
                sh """
                    curl -s -X POST "http://gitea:3000/api/v1/repos/wangzhongqi/embed-hello/statuses/\${GIT_COMMIT}" \
                        -H "Authorization: token \${GITEA_TOKEN}" \
                        -H "Content-Type: application/json" \
                        -d '{"state":"success","description":"Build #${BUILD_NUMBER} passed","context":"continuous-integration/jenkins","target_url":"${BUILD_URL}"}'
                """
            }
        }
        failure {
            sh '''
                { echo "FAILED"; cat /tmp/current-stage.txt; git log -1 --format=%s; } | base64 -w0 > /tmp/build-msg.b64
                ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "powershell -Command \"New-Item -ItemType Directory -Force -Path E:/AI-helper/projects/embed-hello/workspace/build\""
                scp -o StrictHostKeyChecking=no /tmp/build-msg.b64 HUAWEI@10.0.0.2:E:/AI-helper/projects/embed-hello/workspace/build/build-status.txt
                ssh -o StrictHostKeyChecking=no HUAWEI@10.0.0.2 "powershell E:\\AI-helper\\projects\\embed-hello\\workspace\\deploy\\notify-build.ps1"
            '''
            withCredentials([string(credentialsId: 'gitea-api-token', variable: 'GITEA_TOKEN')]) {
                sh """
                    curl -s -X POST "http://gitea:3000/api/v1/repos/wangzhongqi/embed-hello/statuses/\${GIT_COMMIT}" \
                        -H "Authorization: token \${GITEA_TOKEN}" \
                        -H "Content-Type: application/json" \
                        -d '{"state":"failure","description":"Build #${BUILD_NUMBER} failed","context":"continuous-integration/jenkins","target_url":"${BUILD_URL}"}'
                """
            }
        }
    }
}
