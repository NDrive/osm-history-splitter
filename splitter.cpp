
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <getopt.h>
#include <string>
#include <unistd.h>

#include <osmium/io/any_input.hpp>
#include <osmium/io/file.hpp>
#include <osmium/io/reader.hpp>

#include <geos/geom/MultiPolygon.h>
#include <geos/algorithm/locate/IndexedPointInAreaLocator.h>

#include "softcut.hpp"
#include "softercut.hpp"
#include "cut_administrative.hpp"
#include "cut_highway.hpp"
#include "cut_all_borders.hpp"
#include "hardcut.hpp"
#include "cut_ref.hpp"
#include "simplecut.hpp"

template <typename TExtractInfo>
bool readConfig(const std::string& conffile, CutInfo<TExtractInfo> &info) {
    const int linelen = 4096;

    FILE *fp = fopen(conffile.c_str(), "r");
    if (!fp) {
        std::cerr << "unable to open config file " << conffile << "\n";
        return false;
    }

    char line[linelen];
    while (fgets(line, linelen-1, fp)) {
        line[linelen-1] = '\0';
        if (line[0] == '#' || line[0] == '\r' || line[0] == '\n' || line[0] == '\0')
            continue;

        int n = 0;
        char *tok = strtok(line, "\t ");

        const char *name = nullptr;
        double minlon = 0, minlat = 0, maxlon = 0, maxlat = 0;
        char type = '\0';
        char file[linelen];

        while (tok) {
            switch(n) {
                case 0:
                    name = tok;
                    break;

                case 1:
                    if (0 == strcmp("BBOX", tok))
                        type = 'b';
                    else if (0 == strcmp("POLY", tok))
                        type = 'p';
                    else if (0 == strcmp("OSM", tok))
                        type = 'o';
                    else {
                        type = '\0';
                        std::cerr << "output " << name << " of type " << tok << ": unknown output type\n";
                        return false;
                    }
                    break;

                case 2:
                    switch(type) {
                        case 'b':
                            if (4 == sscanf(tok, "%lf,%lf,%lf,%lf", &minlon, &minlat, &maxlon, &maxlat)) {
                                info.addExtract(name, minlat, minlon, maxlat, maxlon);
                            } else {
                                std::cerr << "error reading BBOX " << tok << " for " << name << "\n";
                                return false;
                            }
                            break;
                        case 'p':
                            if (1 == sscanf(tok, "%s", file)) {
                                geos::geom::Geometry *geom = OsmiumExtension::GeometryReader::fromPolyFile(file);
                                if (!geom) {
                                    std::cerr << "error creating geometry from poly-file " << file << " for " << name << "\n";
                                    break;
                                }
                                info.addExtract(name, geom);
                            }
                            break;
                        case 'o':
                            if (1 == sscanf(tok, "%s", file)) {
                                geos::geom::Geometry *geom = OsmiumExtension::GeometryReader::fromOsmFile(file);
                                if (!geom) {
                                    std::cerr << "error creating geometry from poly-file " << file << " for " << name << "\n";
                                    break;
                                }
                                info.addExtract(name, geom);
                            }
                            break;
                    }
                    break;
            }

            tok = strtok(nullptr, "\t ");
            n++;
        }
    }
    fclose(fp);
    return true;
}

int main(int argc, char *argv[]) {
    int cut_algoritm = 3;
    bool debug = false;

    static struct option long_options[] = {
        {"debug",   no_argument, 0, 'd'},
        {"softcut", no_argument, 0, 's'},
        {"hardcut", no_argument, 0, 'h'},
        {"softercut", no_argument, 0, 'r'},
        {"cut_administrative", no_argument, 0, 'c'},
        {"cut_highway", no_argument, 0, 'w'},
        {"cut_all_borders", no_argument, 0, 'b'},
        {"cut_ref", no_argument, 0, 'e'},
        {"simplecut", no_argument, 0, 'p'},
        {0, 0, 0, 0}
    };

    while (true) {
        int c = getopt_long(argc, argv, "dshrcwbep", long_options, 0);
        if (c == -1)
            break;

        switch (c) {
            case 'd':
                debug = true;
                break;
            case 's':
                cut_algoritm = 1;
                break;
            case 'h':
                cut_algoritm = 2;
                break;
            case 'r':
                cut_algoritm = 3;
                break;
	        case 'c':
                cut_algoritm = 4;
                break;
            case 'w':
                cut_algoritm = 5;
                break;
            case 'b':
                cut_algoritm = 6;
                break;
            case 'e':
                cut_algoritm = 7;
                break;
            case 'p':
                cut_algoritm = 8;
                break;

        }
    }

    if (optind > argc-2) {
        std::cerr << "Usage: " << argv[0] << " [OPTIONS] OSMFILE CONFIGFILE\n";
        return 1;
    }

    std::string filename{argv[optind]};
    std::string conffile{argv[optind+1]};

    if ((cut_algoritm == 1 || cut_algoritm == 3 || cut_algoritm == 4 || cut_algoritm == 5 || cut_algoritm == 6 || cut_algoritm == 7 || cut_algoritm == 8) && filename == "-") {
        std::cerr << "Can't read from stdin when in softcut, softercut, cut_administrative, cut_highway, cut_all_borders, simplecut or cut_ref\n";
        return 1;
    }

    osmium::io::File infile(filename);

    if (cut_algoritm == 1) {
        SoftcutInfo info;
        if (!readConfig(conffile, info)) {
            std::cerr << "error reading config\n";
            return 1;
        }

        {
            SoftcutPassOne one(&info);
            one.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, one);
            reader.close();
        }

        {
            SoftcutPassTwo two(&info);
            two.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, two);
            reader.close();
        }

    } else if (cut_algoritm == 2) {
        HardcutInfo info;
        if (!readConfig(conffile, info)) {
            std::cerr << "error reading config\n";
            return 1;
        }

        Hardcut cutter(&info);
        cutter.debug = debug;
        osmium::io::Reader reader(infile);
        osmium::apply(reader, cutter);
        reader.close();

    } else if (cut_algoritm == 3) {
        SoftercutInfo info;
        if (!readConfig(conffile, info)) {
            std::cerr << "error reading config\n";
            return 1;
        }
        {
            SoftercutPassOne one(&info);
            one.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, one);
            reader.close();
        }

        {
            SoftercutPassTwo two(&info);
            two.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, two);
            reader.close();
        }

        {
            SoftercutPassThree three(&info);
            three.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, three);
            reader.close();
        }
    }else if (cut_algoritm == 4) {
        Cut_administrativeInfo info;
        if (!readConfig(conffile, info)) {
            std::cerr << "error reading config\n";
            return 1;
        }
        {
            Cut_administrativePassOne one(&info);
            one.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, one);
            reader.close();
        }

        {
            Cut_administrativePassTwo two(&info);
            two.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, two);
            reader.close();
        }
        {
            Cut_administrativePassThree three(&info);
            three.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, three);
            reader.close();
        }
    }
    else if (cut_algoritm == 5) {
        Cut_highwayInfo info;
        if (!readConfig(conffile, info)) {
            std::cerr << "error reading config\n";
            return 1;
        }
        {
            Cut_highwayPassOne one(&info);
            one.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, one);
            reader.close();
        }

        {
            Cut_highwayPassTwo two(&info);
            two.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, two);
            reader.close();
        }
        {
            Cut_highwayPassThree three(&info);
            three.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, three);
            reader.close();
        }
    }
    else if (cut_algoritm == 6) {
        Cut_all_bordersInfo info;
        if (!readConfig(conffile, info)) {
            std::cerr << "error reading config\n";
            return 1;
        }
        {
            Cut_all_bordersPassOne one(&info);
            one.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, one);
            reader.close();
        }

        {
            Cut_all_bordersPassTwo two(&info);
            two.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, two);
            reader.close();
        }
        {
            Cut_all_bordersPassThree three(&info);
            three.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, three);
            reader.close();
        }
    }
    else if (cut_algoritm == 7) {
        Cut_refInfo info;
        if (!readConfig(conffile, info)) {
            std::cerr << "error reading config\n";
            return 1;
        }
        {
            Cut_refPassOne one(&info);
            one.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, one);
            reader.close();
        }

        {
            Cut_refPassTwo two(&info);
            two.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, two);
            reader.close();
        }
        {
            Cut_refPassThree three(&info);
            three.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, three);
            reader.close();
        }
    }
    else if (cut_algoritm == 8) {
        SimplecutInfo info;
        if (!readConfig(conffile, info)) {
            std::cerr << "error reading config\n";
            return 1;
        }

        {
            SimplecutPassOne one(&info);
            one.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, one);
            reader.close();
        }

        {
            SimplecutPassTwo two(&info);
            two.debug = debug;
            osmium::io::Reader reader(infile);
            osmium::apply(reader, two);
            reader.close();
        }
    }


    return 0;
}

