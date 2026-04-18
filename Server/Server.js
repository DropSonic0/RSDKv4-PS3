const dgram = require('dgram');
const crypto = require('crypto');

const PACKET_SIZE = 512;
const CONNECT_MAGIC = 0x1F2F3F4F;

const ClientHeaders = {
    REQUEST_CODE: 0x00,
    JOIN: 0x01,
    LIST_ROOMS: 0x02,
    DATA: 0x10,
    DATA_VERIFIED: 0x11,
    QUERY_VERIFICATION: 0x20,
    LEAVE: 0xFF
};

const ServerHeaders = {
    CODES: 0x00,
    NEW_PLAYER: 0x01,
    ROOM_LIST: 0x02,
    DATA: 0x10,
    DATA_VERIFIED: 0x11,
    RECEIVED: 0x20,
    VERIFY_CLEAR: 0x21,
    INVALID_HEADER: 0x80,
    NO_ROOM: 0x81,
    UNKNOWN_PLAYER: 0x82,
    LEAVE: 0xFF
};

class Player {
    constructor(rinfo, code, game) {
        this.rinfo = rinfo;
        this.code = code >>> 0;
        this.game = game;
        this.username = "Player";
        this.room = game.getRoom(0);
        game.join(this);
        this.room.players.add(this);
    }

    move(room) {
        this.room.leave(this);
        this.room = room;
        this.room.join(this);
        this.game.rooms.add(this.room);
    }

    deliver(packetBuffer) {
        server.send(packetBuffer, 0, packetBuffer.length, this.rinfo.port, this.rinfo.address);
    }
}

class Room {
    constructor(game, code = 0) {
        this.code = code >>> 0;
        this.players = new Set();
        this.game = game;
        this.verifying = false;
        this.verified = new Set();
        this.vcopy = new Set();
    }

    join(player) {
        if (!this.players.has(player)) {
            this.players.add(player);
            console.log(`[${this.game.name}/${this.code.toString(16).padStart(8, '0').toUpperCase()}] Player ${player.code.toString(16).padStart(8, '0').toUpperCase()} joined`);
        }
    }

    leave(player) {
        if (this.players.has(player)) {
            this.players.delete(player);
            console.log(`[${this.game.name}/${this.code.toString(16).padStart(8, '0').toUpperCase()}] Player ${player.code.toString(16).padStart(8, '0').toUpperCase()} left`);
        }
    }

    deliver(packet, sender) {
        let sent = 0;
        let queueSends = false;
        let pl = Buffer.alloc(0);

        if (packet.header === ClientHeaders.DATA_VERIFIED || packet.header === ClientHeaders.QUERY_VERIFICATION) {
            if (this.vcopy.size === 0) {
                this.vcopy = new Set(this.players);
            }
            if (!this.vcopy.has(sender)) {
                this.vcopy.add(sender);
            }
            for (let p of this.vcopy) {
                if (!this.players.has(p)) this.vcopy.delete(p);
            }

            if (packet.header === ClientHeaders.DATA_VERIFIED) {
                this.verifying = true;
                this.verified.add(sender);
            } else if (!this.verifying) {
                this.verified = new Set(this.players);
            }

            if (setsEqual(this.vcopy, this.verified)) {
                let buffers = [];
                for (let p of this.verified) {
                    let b = Buffer.alloc(8);
                    b.writeUInt32LE(p.code, 0);
                    b.writeUInt32LE(0, 4);
                    buffers.push(b);
                }
                pl = Buffer.concat(buffers);
                queueSends = true;
                this.verified.clear();
                this.vcopy.clear();
                this.verifying = false;
            } else {
                sender.deliver(createPacket(ServerHeaders.RECEIVED, sender.game.bytename, sender.code, this.code, Buffer.alloc(0)));
            }
        }

        if (packet.header !== ClientHeaders.QUERY_VERIFICATION) {
            for (let player of this.players) {
                if (player.code !== sender.code) {
                    player.deliver(packet.buffer);
                    sent++;
                }
            }
        }

        if (queueSends) {
            for (let player of this.players) {
                player.deliver(createPacket(ServerHeaders.VERIFY_CLEAR, this.game.bytename, player.code, this.code, pl));
            }
        }
        return sent;
    }
}

function setsEqual(a, b) {
    if (a.size !== b.size) return false;
    for (let x of a) if (!b.has(x)) return false;
    return true;
}

class Game {
    constructor(bytename) {
        this.bytename = Buffer.alloc(7);
        bytename.copy(this.bytename, 0, 0, 7);
        this.name = bytename.toString().replace(/\0/g, '').trim();
        this.rooms = new Set();
        this.rooms.add(new Room(this, 0));
    }

    join(player) {
        console.log(`[${this.name}] New player ${player.code.toString(16).padStart(8, '0').toUpperCase()}`);
    }

    getRoom(code) {
        code = code >>> 0;
        for (let r of this.rooms) {
            if (r.code === code) return r;
        }
        return this.getRoom(0);
    }
}

const games = new Map();
const players = new Map();

function getGame(bytename) {
    let key = bytename.toString().replace(/\0/g, '').trim();
    if (!games.has(key)) {
        let g = new Game(bytename);
        games.set(key, g);
        console.log(`Game ${g.name} created`);
    }
    return games.get(key);
}

function resolvePlayer(code, gameBytename = null, roomCode = -1) {
    let p = players.get(code >>> 0);
    if (!p) return null;
    if (gameBytename) {
        let name = gameBytename.toString().replace(/\0/g, '').trim();
        if (p.game.name !== name) return null;
    }
    if (roomCode !== -1 && p.room.code !== (roomCode >>> 0)) return null;
    return p;
}

function createPacket(header, game, playerCode, roomCode, data) {
    let buf = Buffer.alloc(PACKET_SIZE);
    buf.writeUInt8(header, 0);
    game.copy(buf, 1, 0, 7);
    buf.writeUInt32LE(playerCode >>> 0, 8);
    buf.writeUInt32LE(roomCode >>> 0, 12);
    if (data) {
        data.copy(buf, 16, 0, Math.min(data.length, PACKET_SIZE - 16));
    }
    return buf;
}

function parsePacket(msg) {
    if (msg.length < 16) return null;
    return {
        header: msg.readUInt8(0),
        game: msg.slice(1, 8),
        player: msg.readUInt32LE(8),
        room: msg.readUInt32LE(12),
        data: msg.slice(16),
        buffer: msg
    };
}

const server = dgram.createSocket('udp4');

server.on('error', (err) => {
    console.log(`Server error:\n${err.stack}`);
    server.close();
});

server.on('message', (msg, rinfo) => {
    const packet = parsePacket(msg);
    if (!packet) return;

    if (packet.player === 0) {
        if (packet.header !== ClientHeaders.REQUEST_CODE && packet.room !== CONNECT_MAGIC) {
            return;
        }
        let code = crypto.randomBytes(4).readUInt32LE(0) >>> 0;
        let p = new Player(rinfo, code, getGame(packet.game));
        players.set(p.code, p);

        if (packet.data && packet.data.length > 0) {
            p.username = packet.data.toString().replace(/\0/g, '').trim();
        }

        p.deliver(createPacket(ServerHeaders.CODES, p.game.bytename, p.code, p.room.code, Buffer.from([0x01, 0x00, 0x00, 0x00])));
        return;
    }

    let p = resolvePlayer(packet.player, packet.game);
    if (!p) {
        if (packet.header !== ClientHeaders.LEAVE) {
            server.send(createPacket(ServerHeaders.UNKNOWN_PLAYER, packet.game, packet.player, packet.room, Buffer.alloc(0)), rinfo.port, rinfo.address);
        }
        return;
    }
    
    p.rinfo = rinfo;

    if (packet.header === ClientHeaders.REQUEST_CODE) {
        if (p.room.code !== 0) {
            let data = Buffer.alloc(4 + (p.room.players.size - 1) * 8);
            data.writeUInt32LE(p.room.players.size, 0);
            let offset = 4;
            for (let other of p.room.players) {
                if (other === p) continue;
                data.writeUInt32LE(other.code, offset);
                data.writeUInt32LE(0, offset + 4);
                offset += 8;
            }
            p.deliver(createPacket(ServerHeaders.CODES, p.game.bytename, p.code, p.room.code, data));
            return;
        }

        let mask = packet.data.readUInt32LE(0);
        let base = packet.data.readUInt32LE(4);
        let existingRoomCodes = Array.from(p.game.rooms).map(r => r.code);
        let roomid;
        do {
            roomid = ((crypto.randomBytes(4).readUInt32LE(0) & ~mask) | base) >>> 0;
        } while (existingRoomCodes.includes(roomid) || roomid === 0);

        let r = new Room(p.game, roomid);
        p.move(r);

        let data = Buffer.alloc(4 + (r.players.size - 1) * 8);
        data.writeUInt32LE(r.players.size, 0);
        let offset = 4;
        for (let other of r.players) {
            if (other === p) continue;
            data.writeUInt32LE(other.code, offset);
            data.writeUInt32LE(0, offset + 4);
            offset += 8;
        }
        p.deliver(createPacket(ServerHeaders.CODES, p.game.bytename, p.code, p.room.code, data));
        return;
    }

    if (packet.header === ClientHeaders.LIST_ROOMS) {
        let availableRooms = [];
        for (let r of p.game.rooms) {
            if (r.code !== 0 && r.players.size === 1) {
                let host = Array.from(r.players)[0];
                availableRooms.push({
                    code: r.code,
                    username: host.username
                });
            }
        }

        console.log(`[${p.game.name}] Room List requested by ${p.code.toString(16).toUpperCase()} (${p.username}). Found ${availableRooms.length} rooms.`);

        let data = Buffer.alloc(4 + availableRooms.length * 24);
        data.writeUInt32LE(availableRooms.length, 0);
        let offset = 4;
        for (let r of availableRooms) {
            data.writeUInt32LE(r.code, offset);
            let nameBuf = Buffer.alloc(20);
            nameBuf.write(r.username.substring(0, 19));
            nameBuf.copy(data, offset + 4);
            offset += 24;
        }
        p.deliver(createPacket(ServerHeaders.ROOM_LIST, p.game.bytename, p.code, 0, data));
        return;
    }

    if (packet.header === ClientHeaders.JOIN) {
        let r = p.game.getRoom(packet.room);
        if (r.code === 0) {
            p.deliver(createPacket(ServerHeaders.NO_ROOM, p.game.bytename, p.code, 0, Buffer.alloc(0)));
            return;
        }
        p.move(r);

        let data = Buffer.alloc(4 + (r.players.size - 1) * 8);
        data.writeUInt32LE(r.players.size, 0);
        let offset = 4;
        for (let other of r.players) {
            if (other === p) continue;
            data.writeUInt32LE(other.code, offset);
            data.writeUInt32LE(0, offset + 4);
            offset += 8;
        }
        p.deliver(createPacket(ServerHeaders.CODES, p.game.bytename, p.code, p.room.code, data));
        r.deliver({header: ServerHeaders.NEW_PLAYER, buffer: createPacket(ServerHeaders.NEW_PLAYER, p.game.bytename, p.code, r.code, Buffer.alloc(0))}, p);
        return;
    }

    if (packet.header === ClientHeaders.DATA || packet.header === ClientHeaders.DATA_VERIFIED || packet.header === ClientHeaders.QUERY_VERIFICATION) {
        if (p.room.code === 0) return;
        p.room.deliver(packet, p);
        return;
    }

    if (packet.header === ClientHeaders.LEAVE) {
        let r = p.room;
        r.leave(p);
        if (r.code === 0) return;
        if (r.players.size > 0) {
            r.deliver(packet, p);
        } else {
            p.game.rooms.delete(r);
            console.log(`[${p.game.name}] Room 0x${r.code.toString(16).padStart(8, '0').toUpperCase()} disbanded`);
        }
        return;
    }
});

server.on('listening', () => {
    const address = server.address();
    console.log(`Server listening ${address.address}:${address.port}`);
});

const PORT = process.env.PORT || 30000;
const HOST = process.env.HOST || '0.0.0.0';

server.bind(PORT, HOST);
