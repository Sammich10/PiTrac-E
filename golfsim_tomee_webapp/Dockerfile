# License Compliance Information
# This Dockerfile creates a container that includes:
# - TomEE (Apache License 2.0)
# - OpenJDK (GPL v2 with Classpath Exception)
# - Maven (Apache License 2.0)
# - ActiveMQ (Apache License 2.0)
# See DOCKER-LICENSE-COMPLIANCE.md for full license details

# Use OpenJDK base image with Debian that supports apt-get
FROM openjdk:17-jdk-slim-bullseye

# Update package list and install required packages
RUN apt-get update && apt-get install -y \
    wget \
    unzip \
    maven \
    && rm -rf /var/lib/apt/lists/*

# Download and install TomEE
RUN cd /tmp && \
    wget https://repo1.maven.org/maven2/org/apache/tomee/apache-tomee/10.0.0/apache-tomee-10.0.0-plume.zip && \
    unzip apache-tomee-10.0.0-plume.zip && \
    mv apache-tomee-plume-10.0.0 /usr/local/tomee && \
    rm -rf apache-tomee-10.0.0-plume.zip

# Set CATALINA_HOME to point to TomEE
ENV CATALINA_HOME=/usr/local/tomee
ENV PATH=$CATALINA_HOME/bin:$PATH

# Reinstall libc-bin from package sources to ensure compatibility
RUN rm -f /var/lib/dpkg/info/libc-bin.* || true
RUN apt-get update && apt-get install -y --reinstall libc-bin && rm -rf /var/lib/apt/lists/*

# Verify Java installation
RUN java -version && javac -version && echo "JAVA_HOME: $JAVA_HOME"

# Set environment variables
ENV PITRAC_ROOT=/app/PiTrac/Software/LMSourceCode \
    PITRAC_BASE_IMAGE_LOGGING_DIR=/app/LM_Shares/Images/ \
    PITRAC_WEBSERVER_SHARE_DIR=/app/LM_Shares/WebShare/ \
    PITRAC_MSG_BROKER_FULL_ADDRESS=tcp://activemq:61616 \
    MAVEN_OPTS=-Xmx1024M

# Create necessary directories
RUN mkdir -p /app/LM_Shares/Images \
    /app/LM_Shares/WebShare \
    /app/Dev/WebAppDev

# Configure TomEE
COPY conf/tomcat-users.xml $CATALINA_HOME/conf/tomcat-users.xml
COPY conf/context.xml $CATALINA_HOME/webapps/manager/META-INF/context.xml
RUN chmod +x $CATALINA_HOME/bin/catalina.sh

# Set up directory structure and files
WORKDIR /app/PiTrac/Software/LMSourceCode/ImageProcessing/golfsim_tomee_webapp
COPY . .

# Create a minimal config file
RUN echo '{ \
    "user_interface": { \
        "kWebServerTomcatShareDirectory": "Images", \
        "kWebServerShareDirectory": "/app/LM_Shares/Images" \
    }, \
    "ipc_interface": { \
        "kWebActiveMQHostAddress": "tcp://activemq:61616" \
    } \
}' > ../golf_sim_config.json

# Convert Windows line endings to Unix and build
WORKDIR /app/Dev/WebAppDev
RUN cp -r /app/PiTrac/Software/LMSourceCode/ImageProcessing/golfsim_tomee_webapp/* . && \
    sed -i 's/\r$//' refresh_from_dev.sh && \
    chmod +x refresh_from_dev.sh && \
    ./refresh_from_dev.sh

# Build and deploy application
RUN mvn package && \
    rm -rf $CATALINA_HOME/webapps/ROOT && \
    cp target/golfsim.war $CATALINA_HOME/webapps/ && \
    cp target/golfsim.war $CATALINA_HOME/webapps/ROOT.war

# Create volume mount points
VOLUME ["/app/LM_Shares/Images", "/app/LM_Shares/WebShare"]

# Copy license compliance documentation
COPY DOCKER-LICENSE-COMPLIANCE.md /usr/share/doc/

# Expose TomEE ports
EXPOSE 8080

# Start TomEE server
CMD ["catalina.sh", "run"]
