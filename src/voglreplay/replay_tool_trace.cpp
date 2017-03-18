/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

// File: replay_tool_trace.cpp
#include <iomanip>
#include <sstream>

#if defined(PLATFORM_OSX)
	#include <unistd.h>
#endif

#include "vogl_common.h"
#include "vogl_gl_replayer.h"
#include "vogl_colorized_console.h"
#include "vogl_file_utils.h"

// Pull in libvogltrace command line options.
#include "../vogltrace/vogl_cmdline_opts.h"

/*
 * To kill dialog asking if you want to launch the game:
 * You also need a steam_dev.cfg in your steam install folder (next to steam.sh) with:
 * bPromptGameLaunchBypass 1
 *
 * in it so you don't get a dialog bugging you before letting the game run.
 */

#if defined(PLATFORM_WINDOWS)

bool
tool_trace_mode(vogl::vector<command_line_param_desc> *desc)
{
    VOGL_VERIFY(!"impl tool_trace_mode");
    return false;
}

#else

#include <libgen.h>

static command_line_param_desc g_command_line_param_descs_dump[] =
{
    // trace specific
    { "dry-run", 0, false, "Don't execute commands." },
    { "xterm", 0, false, "Launch steam url with xterm." },
    { "args", 0, false, "Arguments after are passed as executable arguments." },
};

struct arguments_t
{
    dynamic_string gameid;          // Steam Application ID or local game path.
    dynamic_string game_args;       // Game arguments.
    dynamic_string vogl_cmdline;    // VOGL_CMD_LINE arguments.
    dynamic_string vogl_tracefile;  // vogl tracefile name.
    dynamic_string vogl_logfile;    // vogl logfile name.
    
    bool xterm;                     // Launch steam url with xterm.
    bool dryrun;                    // Don't execute.
};

// Return a game name from a steam appid (hopefully).
static const char *get_game_name(int steam_appid);

static VOGL_NORETURN void
errorf(const char *caller_info, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vogl::console::vprintf(caller_info, vogl::cMsgError, format, args);
    va_end(args);

    exit(-1);
}

static dynamic_string
url_encode(const char *val)
{
    const std::string value = val;
    std::ostringstream escaped;

    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i)
    {
        std::string::value_type c = (*i);

        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            escaped << c;
        else if (c == ' ')
            escaped << "%20";
        else
            escaped << '%' << std::setw(2) << ((int) c) << std::setw(0);
    }

    return escaped.str().c_str();
}

static dynamic_string
getfullpath(const char *filename)
{
    char buf[PATH_MAX];

    // Get the full path to our executable. It should be running in vogl/bin/x86_64 (or i386).
#if defined(PLATFORM_OSX)
    if ((file_utils::get_exec_filename(buf, sizeof(buf)) <= 0))
#else
    if ((readlink("/proc/self/exe", buf, sizeof(buf)) <= 0))
#endif
        errorf(VOGL_FUNCTION_INFO_CSTR, "ERROR: readlink failed '%s.'\n", strerror(errno));

    // Get just the directory name and relative path to libvogltrace.so.
    dynamic_string filedir = dirname(buf);

    filedir += "/";
    filedir += filename;

    // Trim all the relative path parts.
    char *vogltracepath = realpath(filedir.c_str(), NULL);
    if (!vogltracepath)
    {
        vogl_warning_printf("\nWARNING: realpath failed: '%s.' Does '%s' exist?\n", strerror(errno), filename);
        return filename;
    }

    // Make sure our libvogltrace.so exists.
    if(access(vogltracepath, F_OK) != 0)
    {
        vogl_warning_printf("\nWARNING: %s file does not exist.\n", vogltracepath);
        return filename;
    }

    return vogltracepath;
}

bool
tool_trace_mode(vogl::vector<command_line_param_desc> *desc)
{
    arguments_t args;

    if (desc)
    {
        desc->append(g_command_line_param_descs_dump, VOGL_ARRAY_SIZE(g_command_line_param_descs_dump));
        desc->append(g_tracer_cmdline_opts, VOGL_ARRAY_SIZE(g_tracer_cmdline_opts));
        return true;
    }

    // Get steam gameid / local application name.
    args.gameid = g_command_line_params().get_value_as_string_or_empty("", 1);

    if (!args.gameid.size())
        errorf(VOGL_FUNCTION_INFO_CSTR, "ERROR: No application or steamid specified.\n");

    // Get logfile and tracefile names.
    args.vogl_tracefile = g_command_line_params().get_value_as_string("vogl_tracefile");
    args.vogl_logfile = g_command_line_params().get_value_as_string("vogl_logfile");

    args.dryrun = g_command_line_params().get_value_as_bool("dry-run");
    args.xterm = g_command_line_params().get_value_as_bool("xterm");

    // Loop through all the arguments looking for "vogl_*".
    for (vogl::command_line_params::param_map_const_iterator param = g_command_line_params().begin();
         param != g_command_line_params().end();
         ++param)
    {
        const dynamic_string &first = param->first;

        // If this is a vogl command and it's not logfile / tracefile, add it to the vogl_cmdline.
        const dynamic_string prefix = param->first.get_clone().left(5);
        if (prefix == "vogl_" && (first != "vogl_logfile" && first != "vogl_tracefile"))
        {
            args.vogl_cmdline += " --" + first;
            for (uint32_t i = 0; i < param->second.m_values.size(); i++)
            {
                args.vogl_cmdline += " " + param->second.m_values[i];
            }
        }
    }

    // Check for -- or --args and add everything after to game_args.
    const dynamic_string& command_line = get_command_line();

    int args_index = command_line.find_left("-- ");
    if (args_index == -1)
        args_index = command_line.find_left("--args ");
    if (args_index != -1)
    {
        args_index = command_line.find_left(' ', args_index);
        if (args_index != -1)
        {
            args.game_args += command_line.get_clone().right(args_index + 1);
        }
    }

    bool is_steam_file = true;
    if (atoi(args.gameid.c_str()) == 0)
    {
        if (access(args.gameid.c_str(), F_OK))
            errorf(VOGL_FUNCTION_INFO_CSTR, "\nCould not find executable '%s'\n", args.gameid.c_str());

        char *filename = realpath(args.gameid.c_str(), NULL);
        if (filename)
        {
            // This is a local executable.
            is_steam_file = false;
            args.gameid = filename;
            free(filename);
        }
    }

    int steam_appid = is_steam_file ? atoi(args.gameid.c_str()) : -1;
    if (!steam_appid)
        errorf(VOGL_FUNCTION_INFO_CSTR, "ERROR: Could not find game number for %s\n", args.gameid.c_str());

    dynamic_string gameid_str;
    if (is_steam_file)
    {
        gameid_str = "appid" + args.gameid;
        vogl_printf("\nGame AppID: %d", steam_appid);

        const char *game_name = get_game_name(steam_appid);
        if (game_name)
        {
            dynamic_string game_name_str(cVarArg, "%s", game_name);
            vogl_printf(" (%s)", game_name_str.c_str());

            // Trim some characters that don't go well with filenames.
            game_name_str.replace(" ", "_");
            game_name_str.replace(":", "");
            game_name_str.replace("'", "");
            game_name_str.replace("!", "");
            game_name_str.replace("?", "");
            gameid_str += "_" + game_name_str;
        }

        vogl_printf("\n");
    }
    else
    {
        gameid_str = basename((char *)args.gameid.c_str());
        vogl_printf("\nGame: %s\n", args.gameid.c_str());
    }

    // If a tracefile / logfile wasn't specified, set em up.
    if (!args.vogl_tracefile.size() || !args.vogl_logfile.size())
    {
        char timestr[200];
        time_t t = time(NULL);

        timestr[0] = 0;
        struct tm *tmp = localtime(&t);
        if (tmp)
        {
            strftime(timestr, sizeof(timestr), "%Y_%m_%d-%H_%M_%S", tmp);
        }

        dynamic_string fname(cVarArg, "%s/vogltrace.%s.%s", P_tmpdir, gameid_str.c_str(), timestr);
        if (!args.vogl_tracefile.size())
            args.vogl_tracefile = fname + ".bin";
        if (!args.vogl_logfile.size())
            args.vogl_logfile = fname + ".log";
    }

    vogl_printf("\n");
    vogl_message_printf("Tracefile: %s\n", args.vogl_tracefile.c_str());
    vogl_message_printf("Logfile: %s", args.vogl_logfile.c_str());
    vogl_printf(" (will have PID appended)\n");

    dynamic_string vogltracepath = getfullpath((sizeof(void *) < 8) ? "libvogltrace32.so" : "libvogltrace64.so");

    // set up LD_PRELOAD string
    dynamic_string LD_PRELOAD = "LD_PRELOAD=\"";
    LD_PRELOAD += vogltracepath;

    if (is_steam_file || getenv("LD_PRELOAD"))
        LD_PRELOAD += ":$LD_PRELOAD";
    LD_PRELOAD += "\" ";
    vogl_printf("\n%s\n", LD_PRELOAD.c_str());

    // set up VOGL_CMD_LINE string
    dynamic_string VOGL_CMD_LINE = "VOGL_CMD_LINE=\"";
    VOGL_CMD_LINE += "--vogl_tracefile " + args.vogl_tracefile;
    VOGL_CMD_LINE += " --vogl_logfile " + args.vogl_logfile;
    VOGL_CMD_LINE += args.vogl_cmdline;
    VOGL_CMD_LINE += "\"";
    vogl_printf("\n%s\n", VOGL_CMD_LINE.c_str());

    dynamic_string xterm_str;
    if (args.xterm)
    {
        xterm_str = "xterm -geom 120x80+20+20";
        const char *env_user = getenv("USER");

        // If this is mikesart, specify using the Consolas font (which he likes).
        if (env_user && !strcmp(env_user, "mikesart"))
            xterm_str += " -fa Consolas -fs 10";

        // Add the xterm command part.
        xterm_str += " -e ";
    }

    if (is_steam_file)
    {
        // set up xterm string
        dynamic_string steam_cmd_str;

        steam_cmd_str = xterm_str + "%command% " + args.game_args;
        steam_cmd_str.trim();

        // set up steam string
        dynamic_string steam_str = "steam steam://run/" + args.gameid + "//";
        dynamic_string steam_args = VOGL_CMD_LINE + " " + LD_PRELOAD + steam_cmd_str;
        dynamic_string steam_fullcmd = steam_str + url_encode(steam_args.c_str());

        // Spew this whole mess out.
        vogl_printf("\nLaunch string:\n%s\n", (steam_str + steam_args).c_str());
        vogl_printf("\nURL encoded launch string:\n%s\n", steam_fullcmd.c_str());

        // And launch it...
        if (!args.dryrun)
            system(steam_fullcmd.c_str());
    }
    else
    {
        dynamic_string system_cmd;

        system_cmd = VOGL_CMD_LINE + " " + LD_PRELOAD + " " + xterm_str + args.gameid + " " + args.game_args;

        vogl_printf("\nlaunch string:\n%s\n", system_cmd.c_str());

        if (!args.dryrun)
            system(system_cmd.c_str());
    }

    return true;
}

// Current list of Linux Games from http://steamdb.info/linux/
static const char *get_game_name(int steam_appid)
{
    static const struct
    {
        int steam_appid;
        const char *name;
    } s_gameids[] =
    {
        { 230190, "War for the Overworld Bedrock Beta" },
        { 49600, "Beat Hazard" },
        { 234310, "March of War" },
        { 302290, "Infinite Game Works Episode 0" },
        { 238460, "BattleBlock Theater" },
        { 305480, "Heileen 2: The Hands Of Fate" },
        { 204300, "Awesomenauts" },
        { 233450, "Prison Architect" },
        { 251060, "Wargame: Red Dragon" },
        { 41070, "Serious Sam 3: BFE" },
        { 257510, "The Talos Principle" },
        { 237470, "Battle Worlds: Kronos" },
        { 252490, "Rust" },
        { 219890, "Antichamber" },
        { 254200, "FortressCraft Evolved" },
        { 262410, "World of Guns: Gun Disassembly" },
        { 282740, "Organic Panic" },
        { 269210, "Hero Siege" },
        { 251570, "7 Days to Die" },
        { 241240, "Contraption Maker" },
        { 293420, "Heldric - The legend of the shoemaker" },
        { 255500, "Entropy" },
        { 236850, "Europa Universalis IV" },
        { 260160, "The Last Tinker: City of Colors" },
        { 249990, "FORCED" },
        { 214420, "Gear Up" },
        { 235620, "Small World 2" },
        { 212070, "Star Conflict" },
        { 279920, "Infinity Runner" },
        { 219740, "Don't Starve" },
        { 223830, "Xenonauts" },
        { 237870, "Planet Explorers" },
        { 299420, "CubeGun" },
        { 221540, "Defense Grid 2" },
        { 227300, "Euro Truck Simulator 2" },
        { 298340, "Flashout 2" },
        { 236370, "Interstellar Marines" },
        { 234650, "Shadowrun Returns" },
        { 297490, "Spy Chameleon - RGB Agent" },
        { 280520, "Crea" },
        { 265770, "Cannons Lasers Rockets" },
        { 280540, "GhostControl Inc." },
        { 249950, "Forge Quest" },
        { 224740, "Clockwork Empires" },
        { 240760, "Wasteland 2" },
        { 250560, "Fight The Dragon" },
        { 70300, "VVVVVV" },
        { 209190, "Stealth Bastard Deluxe" },
        { 289760, "ROCKETSROCKETSROCKETS" },
        { 291250, "Paragon" },
        { 45100, "Secret of the Magic Crystal" },
        { 248860, "NEO Scavenger" },
        { 209080, "Guns of Icarus Online" },
        { 34440, "Sid Meier's Civilization IV" },
        { 302310, "Dungeon of Elements" },
        { 301460, "Electronic Super Joy Groove City" },
        { 265930, "Goat Simulator" },
        { 250620, "Among the Sleep" },
        { 272670, "Out of the Park Baseball 15" },
        { 269790, "DreadOut" },
        { 242880, "Sir, You Are Being Hunted" },
        { 4000, "Garry's Mod" },
        { 223470, "POSTAL 2" },
        { 227160, "Kinetic Void" },
        { 259680, "Tales of Maj'Eyal" },
        { 270570, "Reversion - The Escape" },
        { 269710, "Tumblestone" },
        { 274520, "Darkwood" },
        { 620, "Portal 2" },
        { 245620, "Tropico 5" },
        { 265240, "Crazy Machines: Golden Gears" },
        { 251470, "TowerFall Ascension" },
        { 281750, "Munin" },
        { 227860, "Castle Story" },
        { 8930, "Sid Meier's Civilization V" },
        { 280220, "Creeper World 3: Arc Eternal" },
        { 298400, "Knightmare Tower" },
        { 258950, "Montague's Mount" },
        { 205990, "Warlock 2: the Exiled" },
        { 300300, "Ichi" },
        { 286660, "Avoid Sensory Overload" },
        { 48700, "Mount & Blade: Warband" },
        { 276730, "Tango Fiesta" },
        { 224860, "Arma Tactics" },
        { 210770, "Sanctum 2" },
        { 236730, "Anomaly 2" },
        { 250050, "Life Goes On" },
        { 299820, "Paradigm Shift" },
        { 274960, "Tech Executive Tycoon" },
        { 220, "Half-Life 2" },
        { 340, "Half-Life 2: Lost Coast" },
        { 420, "Half-Life 2: Episode Two" },
        { 440, "Team Fortress 2" },
        { 280, "Half-Life: Source" },
        { 380, "Half-Life 2: Episode One" },
        { 279520, "Rage Runner" },
        { 268750, "Magicite" },
        { 280040, "A Wizard's Lizard" },
        { 233210, "Air Conflicts: Vietnam" },
        { 200510, "XCOM: Enemy Unknown" },
        { 20920, "The Witcher 2: Assassins of Kings Enhanced Edition" },
        { 262260, "Jets'n'Guns Gold" },
        { 248610, "Door Kickers" },
        { 209060, "Drunken Robot Pornography" },
        { 17520, "Synergy" },
        { 260790, "1001 Spikes" },
        { 290770, "The Fall" },
        { 35720, "Trine 2" },
        { 299860, "Beasts of Prey" },
        { 252550, "Qbeh-1: The Atlas Cube" },
        { 270050, "Quest of Dungeons" },
        { 264140, "Pixel Piracy" },
        { 263060, "Blockstorm" },
        { 570, "Dota 2" },
        { 242680, "Nuclear Throne" },
        { 263080, "Pixel Boy and the Ever Expanding Dungeon" },
        { 25000, "Overgrowth" },
        { 108600, "Project Zomboid" },
        { 221750, "Phaeton: Chapter I" },
        { 284000, "Putt-Putt and Pep's Balloon-o-Rama" },
        { 292860, "Pajama Sam's Lost & Found" },
        { 284020, "Freddi Fish and Luther's Maze Madness" },
        { 283980, "SPY Fox in: Dry Cereal" },
        { 292240, "SPY Fox 2: Some Assembly Required" },
        { 292260, "SPY Fox 3: Operation Ozone" },
        { 292280, "SPY Fox in: Cheese Chase" },
        { 292300, "SPY Fox in: Hold the Mustard" },
        { 283960, "Pajama Sam in No Need to Hide When It's Dark Outside" },
        { 287860, "8-Bit Commando" },
        { 292780, "Pajama Sam 2: Thunder and Lightning Aren't So Frightening" },
        { 292800, "Pajama Sam 3: You Are What You Eat From Your Head To Your Feet" },
        { 292840, "Pajama Sam's Sock Works" },
        { 283940, "Freddi Fish and The Case of the Missing Kelp Seeds" },
        { 294530, "Freddi Fish 2: The Case of the Haunted Schoolhouse" },
        { 294540, "Freddi Fish 3: The Case of the Stolen Conch Shell" },
        { 294550, "Freddi Fish 4: The Case of the Hogfish Rustlers of Briny Gulch" },
        { 294570, "Freddi Fish 5: The Case of the Creature of Coral Cove" },
        { 294580, "Freddi Fish and Luther's Water Worries" },
        { 283920, "Putt-Putt Joins the Parade" },
        { 294650, "Putt-Putt Goes to the Moon" },
        { 294670, "Putt-Putt Travels Through Time" },
        { 294680, "Putt-Putt Enters the Race" },
        { 294690, "Putt-Putt Joins the Circus" },
        { 294710, "Putt-Putt and Pep's Dog on a Stick" },
        { 294720, "Putt-Putt and Fatty Bear's Activity Pack" },
        { 215710, "Fieldrunners 2" },
        { 269310, "Infectonator : Survivors" },
        { 301360, "Kill Fun Yeah" },
        { 280850, "Dollhouse" },
        { 299460, "Woodle Tree Adventures" },
        { 293400, "Epic Space: Online" },
        { 265630, "Fistful of Frags" },
        { 264440, "Children of Liberty" },
        { 207750, "Symphony" },
        { 246070, "Hack 'n' Slash" },
        { 320, "Half-Life 2: Deathmatch" },
        { 286160, "Tabletop Simulator" },
        { 302950, "Heileen 1: Sail Away" },
        { 232050, "Eador. Masters of the Broken World" },
        { 256010, "Jagged Alliance Flashback" },
        { 302380, "Floating Point" },
        { 258760, "Scania Truck Driving Simulator" },
        { 273070, "The Last Federation" },
        { 201310, "X3: Albion Prelude" },
        { 2820, "X3: Terran Conflict" },
        { 293940, "Kill The Bad Guy" },
        { 279940, "The Book of Unwritten Tales 2" },
        { 270450, "Robot Roller-Derby Disco Dodgeball" },
        { 48000, "LIMBO" },
        { 246090, "Spacebase DF-9" },
        { 4920, "Natural Selection 2" },
        { 222880, "Insurgency" },
        { 270150, "RUNNING WITH RIFLES" },
        { 251210, "Hive" },
        { 268360, "Krautscape" },
        { 279260, "Richard & Alice" },
        { 284770, "Enigmatis: The Mists of Ravenwood" },
        { 245470, "Democracy 3" },
        { 250460, "Bridge Constructor" },
        { 271570, "Space Farmers" },
        { 2810, "X3: Reunion" },
        { 301190, "Frederic: Resurrection of Music" },
        { 242860, "Verdun" },
        { 70120, "Hacker Evolution Duality" },
        { 70100, "Hacker Evolution" },
        { 269250, "WORLD END ECONOMiCA episode.01" },
        { 291390, "Three Dead Zed" },
        { 283180, "The Samaritan Paradox" },
        { 99900, "Spiral Knights" },
        { 283660, "Rabbit Hole 3D: Steam Edition" },
        { 233250, "Planetary Annihilation" },
        { 287580, "Pandora: First Contact" },
        { 299780, "Noir Syndrome" },
        { 265830, "Monochroma" },
        { 224760, "FEZ" },
        { 40800, "Super Meat Boy" },
        { 26800, "Braid" },
        { 207080, "Indie Game: The Movie" },
        { 245390, "I Have No Mouth, and I Must Scream" },
        { 215080, "Wakfu" },
        { 288220, "Backstage Pass" },
        { 301200, "Frederic: Evil Strikes Back" },
        { 286380, "Strata" },
        { 228960, "Skulls of the Shogun" },
        { 221180, "Eufloria HD" },
        { 205730, "Insanely Twisted Shadow Planet" },
        { 248510, "Dominions 3" },
        { 243300, "Centration" },
        { 251370, "Escape Goat" },
        { 204240, "The Bridge" },
        { 297120, "Heavy Bullets" },
        { 298480, "Victory At Sea" },
        { 201420, "Toki Tori 2+" },
        { 219680, "Proteus" },
        { 242570, "Space Hulk" },
        { 287600, "Sunset" },
        { 297290, "Detective Case and Clown Bot in: Murder in the Hotel Lisbon" },
        { 264690, "Coin Crypt" },
        { 248570, "Toribash" },
        { 278910, "Interplanetary" },
        { 22140, "Penumbra: Requiem" },
        { 22120, "Penumbra: Black Plague" },
        { 22180, "Penumbra: Overture" },
        { 274290, "Gods Will Be Watching" },
        { 294750, "Anomaly Defenders" },
        { 95000, "Super Splatters" },
        { 285800, "Braveland" },
        { 295630, "Defense Zone 2" },
        { 211900, "Conquest of Elysium 3" },
        { 291430, "Lost Marbles" },
        { 267730, "Ground Pounders" },
        { 234900, "Anodyne" },
        { 293860, "White Noise Online" },
        { 280910, "T.E.C. 3001" },
        { 225420, "Cities in Motion 2" },
        { 290870, "Steam Squad" },
        { 250420, "8BitMMO" },
        { 293920, "Spirited Heart Deluxe" },
        { 226960, "Ironclad Tactics" },
        { 91200, "Anomaly Warzone Earth" },
        { 293200, "Sentinel" },
        { 265890, "Hexcells" },
        { 271900, "Hexcells Plus" },
        { 214870, "Painkiller Hell & Damnation" },
        { 245410, "Wizardry 6: Bane of the Cosmic Forge" },
        { 263460, "Girls Like Robots" },
        { 259470, "Particulars" },
        { 246400, "Nekro" },
        { 294000, "Knight Squad" },
        { 222750, "Wargame: AirLand Battle" },
        { 58610, "Wargame: European Escalation" },
        { 252750, "MouseCraft" },
        { 205910, "Tiny and Big: Grandpa's Leftovers" },
        { 1530, "Multiwinia" },
        { 1520, "DEFCON" },
        { 1510, "Uplink" },
        { 1500, "Darwinia" },
        { 274880, "Jet Car Stunts" },
        { 264060, "Full Bore" },
        { 251630, "The Impossible Game" },
        { 17580, "Dystopia" },
        { 259450, "Drifter" },
        { 43160, "Metro: Last Light" },
        { 250110, "Assault Android Cactus" },
        { 277510, "Shiny The Firefly" },
        { 222730, "Reus" },
        { 212680, "FTL: Faster Than Light" },
        { 257970, "Loren The Amazon Princess" },
        { 286200, "ReignMaker" },
        { 250580, "Paranautical Activity" },
        { 203770, "Crusader Kings II" },
        { 239820, "Game Dev Tycoon" },
        { 274500, "Brigador" },
        { 252630, "Eldritch" },
        { 238630, "Fist Puncher" },
        { 95300, "Capsized" },
        { 216290, "Gateways" },
        { 70110, "Hacker Evolution - Untold" },
        { 255340, "Escape Goat 2" },
        { 268200, "Antisquad" },
        { 268240, "Mechanic Escape" },
        { 246680, "Secrets of Rætikon" },
        { 230700, "La-Mulana" },
        { 265670, "Imagine Me" },
        { 232790, "Broken Age" },
        { 234390, "Teleglitch: Die More Edition" },
        { 295610, "Inescapable" },
        { 15560, "AaaaaAAaaaAAAaaAAAAaAAAAA!!! for the Awesome" },
        { 243280, "Poof" },
        { 237430, "Expeditions: Conquistador" },
        { 286100, "You Have to Win the Game" },
        { 228440, "Cubemen 2" },
        { 63700, "BIT.TRIP BEAT" },
        { 63710, "BIT.TRIP RUNNER" },
        { 218060, "BIT.TRIP Presents... Runner2: Future Legend of Rhythm Alien" },
        { 205070, "BIT.TRIP VOID" },
        { 284390, "The Last Door - Collector's Edition" },
        { 295250, "Stranded" },
        { 292400, "Unrest" },
        { 278440, "0RBITALIS" },
        { 259390, "Ballpoint Universe: Infinite" },
        { 300, "Day of Defeat: Source" },
        { 240, "Counter-Strike: Source" },
        { 269810, "Spate" },
        { 295870, "The Dungeoning" },
        { 282760, "Circuits" },
        { 203560, "Containment: The Zombie Puzzler" },
        { 41800, "Gratuitous Space Battles" },
        { 290990, "Flower Shop: Summer In Fairbrook" },
        { 400, "Portal" },
        { 261350, "Steam Bandits: Outpost" },
        { 92800, "SpaceChem" },
        { 252250, "Maia" },
        { 247870, "Redshirt" },
        { 284790, "Nightmares from the Deep: The Siren`s Call" },
        { 93200, "Revenge of the Titans" },
        { 203210, "Titan Attacks" },
        { 219200, "Droid Assault" },
        { 219190, "Ultratron" },
        { 239070, "Hammerwatch" },
        { 226620, "Desktop Dungeons" },
        { 268810, "Paranormal State: Poison Spring Collector's Edition" },
        { 296930, "Ascendant" },
        { 3830, "Psychonauts" },
        { 220900, "Jack Lumber" },
        { 290060, "Glitchspace" },
        { 286340, "FarSky" },
        { 296850, "Al Emmo and the Lost Dutchman's Mine" },
        { 252410, "SteamWorld Dig" },
        { 257870, "Eschalon: Book 3" },
        { 211820, "Starbound" },
        { 220200, "Kerbal Space Program" },
        { 257930, "Race To Mars" },
        { 283270, "Jagged Alliance Gold" },
        { 47400, "Stronghold 3" },
        { 238910, "Bionic Dues" },
        { 262690, "Little Racers STREET" },
        { 219830, "King Arthur's Gold" },
        { 264380, "Narcissu 1st & 2nd" },
        { 111100, "Snuggle Truck" },
        { 219150, "Hotline Miami" },
        { 259530, "Savant - Ascent" },
        { 291050, "Planet Stronghold" },
        { 291150, "Evolution RTS" },
        { 255920, "The 7th Guest" },
        { 262940, "Broken Sword 5 - the Serpent's Curse" },
        { 248710, "Iesabel" },
        { 231200, "Kentucky Route Zero" },
        { 201040, "Galcon Legends" },
        { 257120, "Not The Robots" },
        { 262590, "Chucks Challenge 3D" },
        { 253030, "Race The Sun" },
        { 233720, "Surgeon Simulator 2013" },
        { 241320, "Ittle Dew" },
        { 111800, "Blocks That Matter" },
        { 209270, "Hero Academy" },
        { 67000, "The Polynomial" },
        { 9500, "Gish" },
        { 65800, "Dungeon Defenders" },
        { 15400, "Harvest: Massive Encounter" },
        { 204060, "Superbrothers: Sword & Sworcery EP" },
        { 22000, "World of Goo" },
        { 27050, "FATALE" },
        { 27020, "The Graveyard" },
        { 1230, "Mare Nostrum" },
        { 1250, "Killing Floor" },
        { 24420, "Aquaria" },
        { 41300, "Altitude" },
        { 22600, "Worms Reloaded" },
        { 18700, "And Yet It Moves" },
        { 26900, "Crayon Physics Deluxe" },
        { 20820, "Shatter" },
        { 1280, "Darkest Hour: Europe '44-'45" },
        { 41900, "The Bard's Tale" },
        { 26500, "Cogs" },
        { 1200, "Red Orchestra: Ostfront 41-45" },
        { 360, "Half-Life Deathmatch: Source" },
        { 10, "Counter-Strike" },
        { 29180, "Osmos" },
        { 30, "Day of Defeat" },
        { 40, "Deathmatch Classic" },
        { 50, "Half-Life: Opposing Force" },
        { 60, "Ricochet" },
        { 37100, "Aztaka" },
        { 6120, "Shank" },
        { 38600, "Faerie Solitaire" },
        { 38700, "Toki Tori" },
        { 38720, "RUSH" },
        { 38740, "EDGE" },
        { 20, "Team Fortress Classic" },
        { 70, "Half-Life" },
        { 80, "Counter-Strike: Condition Zero" },
        { 130, "Half-Life: Blue Shift" },
        { 29160, "Blueberry Garden" },
        { 550, "Left 4 Dead 2" },
        { 17710, "Nuclear Dawn" },
        { 45900, "CID The Dummy" },
        { 45450, "Fortix 2" },
        { 33680, "Eversion" },
        { 61600, "Zen Bound® 2" },
        { 63000, "HOARD" },
        { 65300, "Dustforce" },
        { 18300, "Spectraball" },
        { 206370, "Tales from Space: Mutant Blobs Attack" },
        { 44200, "Galcon Fusion" },
        { 221810, "The Cave" },
        { 221640, "Super Hexagon" },
        { 25010, "Lugaru HD" },
        { 25600, "Eschalon: Book 1" },
        { 35480, "Dwarfs!?" },
        { 221260, "Little Inferno" },
        { 33600, "Broken Sword 2 - the Smoking Mirror: Remastered" },
        { 41100, "Hammerfight" },
        { 25620, "Eschalon: Book 2" },
        { 221830, "The Book of Unwritten Tales: The Critter Chronicles" },
        { 204180, "Waveform" },
        { 204220, "Snapshot" },
        { 80310, "Gemini Rue" },
        { 72000, "Closure" },
        { 57740, "Jagged Alliance - Back in Action" },
        { 57640, "Broken Sword 1 - Shadow of the Templars: Director's Cut" },
        { 57300, "Amnesia: The Dark Descent" },
        { 96200, "Steel Storm: Burning Retribution" },
        { 202410, "Scoregasm" },
        { 209790, "Splice" },
        { 206440, "To the Moon" },
        { 73010, "Cities in Motion" },
        { 97000, "Solar 2" },
        { 206690, "iBomber Defense Pacific" },
        { 220090, "The Journey Down: Chapter One" },
        { 215510, "Rocketbirds: Hardboiled Chicken" },
        { 220460, "Cargo Commander" },
        { 220780, "Thomas Was Alone" },
        { 202730, "Dynamite Jack" },
        { 221020, "Towns" },
        { 207170, "Legend of Grimrock" },
        { 209370, "Analogue: A Hate Story" },
        { 225260, "Brütal Legend" },
        { 98100, "TRAUMA" },
        { 217290, "Din's Curse" },
        { 215160, "The Book of Unwritten Tales" },
        { 214970, "Intrusion 2" },
        { 105100, "Lume" },
        { 102840, "Shank 2" },
        { 107100, "Bastion" },
        { 211340, "Magical Diary" },
        { 98800, "Dungeons of Dredmor" },
        { 207250, "Cubemen" },
        { 209540, "Strike Suit Zero" },
        { 98200, "Frozen Synapse" },
        { 207570, "English Country Tune" },
        { 207690, "Botanicula" },
        { 207650, "A Virus Named TOM" },
        { 227200, "Waking Mars" },
        { 111000, "The Clockwork Man" },
        { 111010, "The Clockwork Man: The Hidden World" },
        { 108200, "Ticket to Ride" },
        { 214910, "Air Conflicts: Pacific Carriers" },
        { 214560, "Mark of the Ninja" },
        { 107200, "Space Pirates and Zombies" },
        { 115100, "Costume Quest" },
        { 222140, "Puddle" },
        { 115110, "Stacking" },
        { 200390, "Oil Rush" },
        { 200900, "Cave Story+" },
        { 107800, "Rochard" },
        { 112100, "Avadon: The Black Fortress" },
        { 113020, "Monaco" },
        { 213650, "Dwarfs F2P" },
        { 217690, "Anna - Extended Edition" },
        { 210170, "Spirits" },
        { 218090, "Unity of Command" },
        { 218410, "Defender's Quest: Valley of the Forgotten" },
        { 218660, "iBomber Attack" },
        { 227580, "10,000,000" },
        { 248470, "Doorways" },
        { 235000, "The Raven - Legacy of a Master Thief Preview" },
        { 294230, "Millie" },
        { 293300, "LEVEL 22" },
        { 292090, "Rube Works" },
        { 290750, "Earth: Year 2066" },
        { 291030, "Always Remember Me" },
        { 290280, "Volt" },
        { 287020, "Harvester" },
        { 284830, "Clockwork Tales: Of Glass and Ink" },
        { 284710, "Abyss: The Wraiths of Eden" },
        { 282880, "FaeVerse Alchemy" },
        { 283040, "Paper Dungeons" },
        { 280830, "Foosball - Street Edition" },
        { 280500, "KRUNCH" },
        { 279160, "Ultionus: A Tale of Petty Revenge" },
        { 278930, "Gigantic Army" },
        { 274450, "The Powerpuff Girls: Defenders of Townsville" },
        { 274480, "Drox Operative" },
        { 274980, "Influent" },
        { 272060, "Serena" },
        { 271820, "Card City Nights" },
        { 270750, "Realms of Arkania 2 - Star Trail Classic" },
        { 270760, "Realms of Arkania 3 - Shadows over Riva Classic" },
        { 270330, "Ku: Shroud of the Morrigan" },
        { 269150, "Luxuria Superbia" },
        { 269010, "Science Girls" },
        { 267670, "Realms of Arkania 1 - Blade of Destiny Classic" },
        { 267060, "Gravity Badgers" },
        { 266130, "Breach & Clear" },
        { 266090, "Starlite: Astronaut Rescue" },
        { 265380, "Grimind" },
        { 266010, "LYNE" },
        { 264730, "Deadly 30" },
        { 264340, "Major Mayhem" },
        { 264280, "99 Levels To Hell" },
        { 263840, "Out of the Park Baseball 14" },
        { 264080, "Vangers" },
        { 264160, "WazHack" },
        { 263420, "Probably Archery" },
        { 263360, "3089 -- Futuristic Action RPG" },
        { 263320, "Saturday Morning RPG" },
        { 263340, "Continue?9876543210" },
        { 261700, "Eryi's Action" },
        { 261680, "Journal" },
        { 261820, "Estranged: Act I" },
        { 261960, "Cube & Star: An Arbitrary Love" },
        { 259740, "Nightmares from the Deep: The Cursed Heart" },
        { 259720, "Fading Hearts" },
        { 259780, "Nimble Quest" },
        { 259830, "Wooden Sen'SeY" },
        { 259430, "Zigfrak" },
        { 259620, "3079 -- Block Action RPG" },
        { 259600, "Finding Teddy" },
        { 259510, "Shufflepuck Cantina Deluxe" },
        { 259700, "Dead Sky" },
        { 259060, "Dominions 4" },
        { 259130, "Wasteland 1 - The Original Classic" },
        { 258890, "Type:Rider" },
        { 257830, "Violett" },
        { 258050, "Survivor Squad" },
        { 257770, "Signal Ops" },
        { 257670, "Elder Sign: Omens" },
        { 257630, "Blast Em!" },
        { 255870, "PixelJunk™ Shooter" },
        { 255300, "Journey of a Roach" },
        { 253790, "rymdkapsel" },
        { 253650, "Sparkle 2 Evo" },
        { 252670, "Nihilumbra" },
        { 253110, "The Cat Lady" },
        { 253310, "Fester Mudd: Curse of the Gold - Episode 1" },
        { 253410, "Ravensword: Shadowlands" },
        { 253570, "Gentlemen!" },
        { 252310, "Syder Arcade" },
        { 252170, "Anomaly Warzone Earth Mobile Campaign" },
        { 252010, "Oniken" },
        { 251990, "Long Live The Queen" },
        { 252470, "Space Pirates and Zombies 2" },
        { 251910, "Solar Flux" },
        { 252370, "The Shivah" },
        { 251650, "Ray's The Dead" },
        { 251410, "Dark Matter" },
        { 251530, "Anomaly Korea" },
        { 251710, "Chainsaw Warrior" },
        { 250600, "The Plan" },
        { 250440, "Tetrapulse" },
        { 250380, "Knock-knock" },
        { 250260, "Jazzpunk" },
        { 249630, "Delver" },
        { 249570, "The Castle Doctrine" },
        { 249590, "Teslagrad" },
        { 248800, "Dysfunctional Systems: Learning to Manage Chaos" },
        { 248550, "Megabyte Punch" },
        { 248410, "Legends of Aethereus" },
        { 248190, "Knytt Underground" },
        { 248450, "Salvation Prophecy" },
        { 247020, "Cook, Serve, Delicious!" },
        { 246800, "BeatBlasters III" },
        { 246420, "Kingdom Rush" },
        { 246360, "Perfection." },
        { 245550, "Free to Play" },
        { 245430, "Wizardry 7: Crusaders of the Dark Savant" },
        { 245150, "The Novelist" },
        { 244870, "Electronic Super Joy" },
        { 244810, "Foul Play" },
        { 243780, "PixelJunk™ Monsters Ultimate" },
        { 243140, "Glare" },
        { 242820, "140" },
        { 242530, "The Chaos Engine" },
        { 242110, "Joe Danger 2: The Movie" },
        { 241640, "Haunted Memories" },
        { 241600, "Rogue Legacy" },
        { 240660, "RainBlood Chronicles: Mirage" },
        { 239700, "Hate Plus" },
        { 239200, "Amnesia: A Machine for Pigs" },
        { 239030, "Papers, Please" },
        { 238280, "Legend of Dungeon" },
        { 238210, "System Shock 2" },
        { 237550, "Realms of Arkania: Blade of Destiny" },
        { 235980, "Tetrobot and Co." },
        { 236090, "Dust: An Elysian Tail" },
        { 235820, "Element4l" },
        { 234940, "The 39 Steps" },
        { 234190, "Receiver" },
        { 234000, "Superfrog HD" },
        { 234060, "Timelines: Assault on America" },
        { 233740, "Organ Trail: Director's Cut" },
        { 233370, "The Raven - Legacy of a Master Thief" },
        { 233150, "LUFTRAUSERS" },
        { 233230, "Kairo" },
        { 232770, "POSTAL" },
        { 232430, "Gone Home" },
        { 231720, "Bad Hotel" },
        { 231740, "Knights of Pen and Paper +1" },
        { 231910, "Leisure Suit Larry in the Land of the Lounge Lizards: Reloaded" },
        { 231670, "Football Manager 2014" },
        { 231330, "Deadfall Adventures" },
        { 231310, "MirrorMoon EP" },
        { 231160, "The Swapper" },
        { 230980, "Starseed Pilgrim" },
        { 231040, "Beatbuddy: Tale of the Guardians" },
        { 230760, "DIVO" },
        { 230310, "Akaneiro: Demon Hunters" },
        { 229890, "Joe Danger" },
        { 229520, "Dungeon Hearts" },
        { 226740, "Monster Loves You!" },
        { 225140, "Duke Nukem 3D: Megaton Edition" },
        { 224880, "Equate Game" },
        { 225160, "Shadow Warrior Classic Redux" },
        { 224480, "Octodad: Dadliest Catch" },
        { 224260, "No More Room in Hell" },
        { 222520, "Champions of Regnum" },
        { 214770, "Guacamelee! Gold Edition" },
        { 214550, "Eets Munchies" },
        { 100,"Counter-Strike: Condition Zero Deleted Scenes" },
        { 710, "Counter-Strike: Global Offensive Beta" },
        { 730, "Counter-Strike: Global Offensive" },
        { 13700, "Savage 2: A Tortured Soul" },
        { 223530, "Left 4 Dead 2 Beta" },
        { 205790, "Dota 2 Test" },
        { 239760, "Football Manager Classic 2014, 2 months ago, Signs of Linux Support" },
        { 99920, "Spiral Knights Preview, 5 months ago, Game Possibly Works" },
    };

    for (size_t i = 0; i < sizeof(s_gameids) / sizeof(s_gameids[0]); i++)
    {
        if (s_gameids[i].steam_appid == steam_appid)
            return s_gameids[i].name;
    }

    return NULL;
}

#endif // LINUX
