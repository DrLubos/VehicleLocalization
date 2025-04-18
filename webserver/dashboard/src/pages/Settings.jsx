// src/pages/Settings.jsx
import React, { useState } from "react";
import {
    Box, Heading, VStack, FormControl, FormLabel,
    Input, Button, Tabs, TabList, TabPanels, Tab, TabPanel,
    useToast, useColorModeValue,
} from "@chakra-ui/react";
import Navbar from "../components/Navbar";

const Settings = () => {
    const toast = useToast();
    // email form state
    const [newEmail, setNewEmail] = useState("");
    const [emailPwd, setEmailPwd] = useState("");
    // password form state
    const [oldPassword, setOldPassword] = useState("");
    const [newPassword, setNewPassword] = useState("");
    const [confirmPassword, setConfirmPassword] = useState("");

    const token = localStorage.getItem("authToken");
    const headers = { "Content-Type": "application/json", Authorization: `Bearer ${token}` };

    const handleEmailChange = async () => {
        try {
            const res = await fetch("https://webapi.vehiclemap.xyz/user/email", {
                method: "PUT",
                headers,
                body: JSON.stringify({ new_email: newEmail, password: emailPwd }),
            });
            if (!res.ok) {
                const errBody = await res.json().catch(() => ({}));
                let msg = errBody.detail;
                if (Array.isArray(msg)) {
                    msg = msg[0].msg || msg[0];
                }
                toast({ status: "error", title: "Error", description: msg || "Unknown error." });
            } else{
                toast({ status: "success", title: "Success", description: "Email updated successfully!" });
            }
        } catch (e) {
            toast({ status: "error", title: "Error", description: e.message });
        }
    };

    const handlePasswordChange = async () => {
        try {
            const res = await fetch("https://webapi.vehiclemap.xyz/user/password", {
                method: "PUT",
                headers,
                body: JSON.stringify({ old_password: oldPassword, new_password: newPassword, confirm_password: confirmPassword }),
            });
            if (!res.ok) {
                const errBody = await res.json().catch(() => ({}));
                let msg = errBody.detail;
                if (Array.isArray(msg)) {
                    msg = msg[0].msg || msg[0];
                }
                toast({ status: "error", title: "Error", description: msg || "Unknown error." });
            } else {
                toast({ status: "success", title: "Success", description: "Password updated successfully!" });
            }
        } catch (e) {
            toast({ status: "error", title: "Error", description: e.message });
        }
    };

    return (
        <Box minH="100vh" bg={useColorModeValue("gray.50", "gray.900")}>
            <Navbar />
            <Box maxW="md" mx="auto" mt={8}>
                <Heading textAlign="center" mb={4}>Settings</Heading>
                <Tabs
                    isFitted
                    variant="enclosed"
                    onChange={(index) => {
                        if (index === 0) {
                            setNewEmail("");
                            setEmailPwd("");
                        } else {
                            setOldPassword("");
                            setNewPassword("");
                            setConfirmPassword("");
                        }
                    }}
                >
                    <TabList mb="1em">
                        <Tab>Change Email</Tab>
                        <Tab>Change Password</Tab>
                    </TabList>
                    <TabPanels>
                        <TabPanel>
                            <VStack spacing={4} align="stretch">
                                <FormControl>
                                    <FormLabel>New Email</FormLabel>
                                    <Input value={newEmail} onChange={e => setNewEmail(e.target.value)} />
                                </FormControl>
                                <FormControl>
                                    <FormLabel>Password</FormLabel>
                                    <Input type="password" value={emailPwd} onChange={e => setEmailPwd(e.target.value)} />
                                </FormControl>
                                <Button colorScheme="blue" onClick={handleEmailChange}>Update Email</Button>
                            </VStack>
                        </TabPanel>
                        <TabPanel>
                            <VStack spacing={4} align="stretch">
                                <FormControl>
                                    <FormLabel>Old Password</FormLabel>
                                    <Input type="password" value={oldPassword} onChange={e => setOldPassword(e.target.value)} />
                                </FormControl>
                                <FormControl>
                                    <FormLabel>New Password</FormLabel>
                                    <Input type="password" value={newPassword} onChange={e => setNewPassword(e.target.value)} />
                                </FormControl>
                                <FormControl>
                                    <FormLabel>Confirm New Password</FormLabel>
                                    <Input type="password" value={confirmPassword} onChange={e => setConfirmPassword(e.target.value)} />
                                </FormControl>
                                <Button colorScheme="blue" onClick={handlePasswordChange}>Update Password</Button>
                            </VStack>
                        </TabPanel>
                    </TabPanels>
                </Tabs>
            </Box>
        </Box>
    );
};

export default Settings;