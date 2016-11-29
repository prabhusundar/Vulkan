import os

####################
#
# Functions
#
####################
        
        
####################
#
# Main
#
####################

print("Creating master CMakeLists for all make projects")

noVisual = ["VKTS_Example08"]

a = open("CMakeLists.txt", "w")
a.write("cmake_minimum_required(VERSION 3.2)\n")
a.write("project(\"VKTS\")\n")

print("Processing 'VKTS'")
a.write("add_subdirectory( VKTS ) \n")

allExamples = os.listdir()
for example in allExamples:

        if example in noVisual:
            continue
        
        if example.startswith("VKTS_Example"):
            a.write("add_subdirectory( " + example + " )\n" )
            print("Processing '%s'" % (example))
