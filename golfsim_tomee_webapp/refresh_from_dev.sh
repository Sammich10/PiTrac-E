# After running this script, then do a "mvn package" to compile and then  
# /opt/tomee/bin/restart.sh  
mkdir -p src/main/webapp/WEB-INF
mkdir -p src/main/java/com/verdanttechs/jakarta/ee9
cp $PITRAC_ROOT/ImageProcessing/golfsim_tomee_webapp/src/main/java/com/verdanttechs/jakarta/ee9/MonitorServlet.java ./src/main/java/com/verdanttechs/jakarta/ee9/  
cp $PITRAC_ROOT/ImageProcessing/golfsim_tomee_webapp/src/main/webapp/WEB-INF/*.jsp ./src/main/webapp/WEB-INF  
cp $PITRAC_ROOT/ImageProcessing/golfsim_tomee_webapp/src/main/webapp/*.html ./src/main/webapp  
cp $PITRAC_ROOT/ImageProcessing/golfsim_tomee_webapp/pom.xml .  
# Also pull over the current .json configuration file to make sure that the webapp is looking at the correct version.  
cp $PITRAC_ROOT/ImageProcessing/golf_sim_config.json $PITRAC_WEBSERVER_SHARE_DIR

#Point the URL for the PiTrac Gui to the right configuration file location
echo $PITRAC_WEBSERVER_SHARE_DIR > webserver_name.tmp.txt
sed -i 's/\//%2F/g' webserver_name.tmp.txt

sed -i 's@PITRAC_WEBSERVER_SHARE_DIR@'`cat webserver_name.tmp.txt`'@g' ./src/main/webapp/index.html

rm webserver_name.tmp.txt

sed -i 's@PITRAC_MSG_BROKER_FULL_ADDRESS@'$PITRAC_MSG_BROKER_FULL_ADDRESS'@g' $PITRAC_WEBSERVER_SHARE_DIR/golf_sim_config.json
